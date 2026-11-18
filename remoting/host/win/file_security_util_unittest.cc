// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/file_security_util.h"

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/process/process_info.h"
#include "base/strings/cstring_view.h"
#include "base/test/bind.h"
#include "base/test/file_path_reparse_point_win.h"
#include "base/types/expected.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

constexpr DWORD kShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr DWORD kNoFollowFlags =
    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS;
constexpr SECURITY_INFORMATION kOwnerAndDacl =
    OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;

constexpr wchar_t kAdminOnlySddl[] = L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)";
constexpr wchar_t kInheritableAdminOnlySddl[] =
    L"O:BAG:BAD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)";
constexpr wchar_t kDenyAdminSddl[] = L"D:P(D;;GA;;;BA)";

base::win::SecurityDescriptor SdFromSddl(base::wcstring_view sddl) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(sddl);
  CHECK(sd.has_value());
  return std::move(*sd);
}

base::win::Sid GetCurrentUser() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  CHECK(token.has_value());
  return token->User();
}

// Returns an SDDL string whose owner and group are the current process user
// and whose DACL (with `dacl_flags`, e.g. `P`) grants Generic All with
// `ace_flags` (e.g. `OICI`) to `SY`, `BA`, and the current user. Setting the
// token user as owner needs no privilege, so tests using this run unelevated.
std::wstring UserOwnedSddl(std::wstring_view dacl_flags,
                           std::wstring_view ace_flags) {
  std::optional<std::wstring> user = GetCurrentUser().ToSddlString();
  CHECK(user.has_value());
  const std::wstring flags(ace_flags);
  return L"O:" + *user + L"G:" + *user + L"D:" + std::wstring(dacl_flags) +
         L"(A;" + flags + L";GA;;;SY)(A;" + flags + L";GA;;;BA)(A;" + flags +
         L";GA;;;" + *user + L")";
}

bool IsCurrentUserTrustedPrincipal() {
  const base::win::Sid user = GetCurrentUser();
  return user == base::win::Sid(base::win::WellKnownSid::kLocalSystem) ||
         user ==
             base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators);
}

bool IsPrivilegeEnabledOnProcess(const wchar_t* privilege_name) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token.has_value()) {
    return false;
  }
  for (const base::win::AccessToken::Privilege& privilege :
       token->Privileges()) {
    if (privilege.GetName() == privilege_name) {
      return privilege.IsEnabled();
    }
  }
  return false;
}

bool IsCurrentThreadImpersonating() {
  return base::win::AccessToken::FromCurrentThread().has_value();
}

// Returns the current process user if the test is running elevated as a
// non-BA/SY TokenUser, so that an untrusted owner can be simulated.
std::optional<base::win::Sid> GetUntrustedOwnerForTesting() {
  if (!base::IsCurrentProcessElevated() || IsCurrentUserTrustedPrincipal()) {
    return std::nullopt;
  }
  return GetCurrentUser();
}

// Sets `owner` as Owner and a protected DACL denying `BA` all access on `path`.
void ApplyDenyAdminSd(const base::FilePath& path, const base::win::Sid& owner) {
  base::win::SecurityDescriptor sd = SdFromSddl(kDenyAdminSddl);
  sd.set_owner(owner);
  ASSERT_TRUE(sd.WriteToFile(path, kOwnerAndDacl));
}

void ExpectOwnerAndProtectedDacl(const base::FilePath& path,
                                 const base::win::Sid& expected_owner) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(path, kOwnerAndDacl);
  ASSERT_TRUE(sd.has_value());
  ASSERT_TRUE(sd->owner().has_value());
  EXPECT_EQ(*sd->owner(), expected_owner);
  EXPECT_TRUE(sd->dacl_protected());
}

// Returns the owner/DACL SDDL of the object at `path`, without following a
// reparse point at `path` itself.
std::optional<std::wstring> GetOwnerAndDaclSddl(const base::FilePath& path) {
  base::win::ScopedHandle handle(
      ::CreateFileW(path.value().c_str(), READ_CONTROL, kShareAll, nullptr,
                    OPEN_EXISTING, kNoFollowFlags, nullptr));
  if (!handle.is_valid()) {
    return std::nullopt;
  }
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromHandle(
          handle.Get(), base::win::SecurityObjectType::kFile, kOwnerAndDacl);
  if (!sd.has_value()) {
    return std::nullopt;
  }
  return sd->ToSddl(kOwnerAndDacl);
}

}  // namespace

class FileSecurityUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::win::ScopedHandle root_handle(::CreateFileW(
        temp_dir_.GetPath().value().c_str(), FILE_READ_ATTRIBUTES, kShareAll,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
    ASSERT_TRUE(root_handle.is_valid());
    std::optional<base::FilePath> canonical_temp =
        GetCanonicalPath(root_handle.Get());
    ASSERT_TRUE(canonical_temp.has_value());
    canonical_temp_ = *canonical_temp;
  }

  // Creates a directory at `name` with `sddl` and runs
  // `InspectFileSecurityDescriptor` on it with `policy`.
  void InspectNewDirectory(const wchar_t* name,
                           base::wcstring_view sddl,
                           const SecurityDescriptorPolicy& policy,
                           SecurityDescriptorInspection* out_result) {
    base::FilePath dir = temp_dir_.GetPath().Append(name);
    std::optional<base::win::SecurityDescriptor::SelfRelative> sd =
        SdFromSddl(sddl).ToSelfRelative();
    ASSERT_TRUE(sd.has_value());
    SECURITY_ATTRIBUTES sa = {sizeof(sa), sd->get(), FALSE};
    ASSERT_TRUE(::CreateDirectoryW(dir.value().c_str(), &sa));
    base::win::ScopedHandle h(::CreateFileW(
        dir.value().c_str(), READ_CONTROL | FILE_READ_ATTRIBUTES, kShareAll,
        nullptr, OPEN_EXISTING, kNoFollowFlags, nullptr));
    ASSERT_TRUE(h.is_valid());
    std::optional<SecurityDescriptorInspection> result =
        InspectFileSecurityDescriptor(h.Get(), policy);
    ASSERT_TRUE(result.has_value());
    *out_result = *result;
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath canonical_temp_;
};

TEST_F(FileSecurityUtilTest, ToExtendedLengthPath) {
  auto convert = [](const wchar_t* path) {
    return ToExtendedLengthPath(base::FilePath(path));
  };
  EXPECT_EQ(convert(L"C:\\a\\b."), base::FilePath(L"\\\\?\\C:\\a\\b."));
  EXPECT_EQ(convert(L"C:\\a\\b "), base::FilePath(L"\\\\?\\C:\\a\\b "));
  EXPECT_EQ(convert(L"C:/a/b"), base::FilePath(L"\\\\?\\C:\\a\\b"));
  EXPECT_EQ(convert(L"\\\\server\\share\\a"),
            base::FilePath(L"\\\\?\\UNC\\server\\share\\a"));

  // Already-extended drive, UNC, and volume GUID paths are accepted (with
  // separators normalized).
  EXPECT_EQ(convert(L"\\\\?\\C:\\a"), base::FilePath(L"\\\\?\\C:\\a"));
  EXPECT_EQ(convert(L"\\\\?\\C:/a/b."), base::FilePath(L"\\\\?\\C:\\a\\b."));
  EXPECT_EQ(convert(L"\\\\?\\UNC\\server\\share\\a"),
            base::FilePath(L"\\\\?\\UNC\\server\\share\\a"));
  constexpr wchar_t kVolumePath[] =
      L"\\\\?\\Volume{01234567-89ab-cdef-0123-456789ABCDEF}\\a";
  EXPECT_EQ(convert(kVolumePath), base::FilePath(kVolumePath));

  // Components made only of dots/spaces other than `.` and `..` are literal
  // names in extended-length form.
  EXPECT_EQ(convert(L"C:\\a\\..."), base::FilePath(L"\\\\?\\C:\\a\\..."));
  EXPECT_EQ(convert(L"C:\\a\\.. "), base::FilePath(L"\\\\?\\C:\\a\\.. "));
  EXPECT_EQ(convert(L"\\\\?\\C:\\a\\..."),
            base::FilePath(L"\\\\?\\C:\\a\\..."));

  // Paths that cannot be safely expressed in extended-length form fail closed.
  EXPECT_FALSE(convert(L"relative\\a").has_value());
  EXPECT_FALSE(convert(L"C:\\a\\..\\b").has_value());
  EXPECT_FALSE(convert(L"C:\\a\\.\\b").has_value());
  EXPECT_FALSE(convert(L"C:\\a\\.").has_value());
  EXPECT_FALSE(convert(L"\\\\.\\C:\\a").has_value());
  EXPECT_FALSE(convert(L"\\\\\\share\\a").has_value());

  // Already-extended paths are rejected unless they are drive, UNC, or volume
  // GUID paths without `.` or `..` components.
  EXPECT_FALSE(convert(L"\\\\?\\pipe\\x").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\PhysicalDrive0").has_value());
  EXPECT_FALSE(
      convert(L"\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\a").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\C:").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\UNC\\\\share").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\UNC\\server").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\Volume{not-a-guid}\\a").has_value());
  // Volume GUIDs of the right length with a non-hex digit, a misplaced dash, a
  // wrong closing brace, or no separator after the closing brace.
  EXPECT_FALSE(
      convert(L"\\\\?\\Volume{0123456g-89ab-cdef-0123-456789abcdef}\\a")
          .has_value());
  EXPECT_FALSE(
      convert(L"\\\\?\\Volume{012345678-9ab-cdef-0123-456789abcdef}\\a")
          .has_value());
  EXPECT_FALSE(
      convert(L"\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef)\\a")
          .has_value());
  EXPECT_FALSE(convert(L"\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}a")
                   .has_value());
  EXPECT_FALSE(convert(L"\\\\?\\Volume{01234567-89ab-cdef-0123-456789abcdef}")
                   .has_value());
  EXPECT_FALSE(convert(L"\\\\?\\C:\\a\\..\\b").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\C:\\a\\.\\b").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\C:\\a\\.").has_value());
  EXPECT_FALSE(convert(L"\\\\?\\UNC\\server\\share\\..").has_value());
}

TEST_F(FileSecurityUtilTest, CanonicalPathResolutionAndVerification) {
  base::FilePath real_dir = temp_dir_.GetPath().Append(L"real_dir");
  ASSERT_TRUE(base::CreateDirectory(real_dir));

  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_dir");
  ASSERT_TRUE(base::CreateDirectory(junction_dir));
  std::optional<base::test::FilePathReparsePoint> rp =
      base::test::FilePathReparsePoint::Create(junction_dir, real_dir);
  ASSERT_TRUE(rp.has_value());

  // Opening `junction_dir` WITHOUT `FILE_FLAG_OPEN_REPARSE_POINT` resolves to
  // `real_dir`'s canonical path.
  base::win::ScopedHandle resolved_handle(::CreateFileW(
      junction_dir.value().c_str(), FILE_READ_ATTRIBUTES, kShareAll, nullptr,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  ASSERT_TRUE(resolved_handle.is_valid());

  std::optional<base::FilePath> canonical_real =
      GetCanonicalPath(resolved_handle.Get());
  ASSERT_TRUE(canonical_real.has_value());
  EXPECT_EQ(*canonical_real, canonical_temp_.Append(L"real_dir"));
  EXPECT_TRUE(IsExpectedCanonicalPath(resolved_handle.Get(), *canonical_real));
  EXPECT_FALSE(IsExpectedCanonicalPath(
      resolved_handle.Get(), canonical_real->DirName().Append(L"other_dir")));
}

TEST_F(FileSecurityUtilTest, InspectFileSecurityDescriptor_OwnerAndDaclChecks) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  SecurityDescriptorInspection result;
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"strict", kAdminOnlySddl, {}, &result));
  EXPECT_TRUE(result.trusted_owner);
  EXPECT_TRUE(result.safe_dacl);

  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"permissive", L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GW;;;BU)", {},
      &result));
  EXPECT_TRUE(result.trusted_owner);
  EXPECT_FALSE(result.safe_dacl);
}

TEST_F(FileSecurityUtilTest,
       InspectFileSecurityDescriptor_UserOwnerIsUntrusted) {
  if (IsCurrentUserTrustedPrincipal()) {
    GTEST_SKIP() << "Requires a TokenUser other than SY/BA.";
  }
  std::optional<std::wstring> user = GetCurrentUser().ToSddlString();
  ASSERT_TRUE(user.has_value());

  // The token user is not trusted as owner, even when it is a member of `BA`.
  // Setting the token user as owner needs no privilege, so this runs
  // unelevated. The user is granted only `SYNCHRONIZE` (which `CreateFileW`
  // always requests), and that is within `allowed_non_admin_mask`, so the DACL
  // itself is safe.
  const std::wstring sddl = L"O:" + *user +
                            L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x100000;;;" +
                            *user + L")";
  SecurityDescriptorInspection result;
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"user_owned", sddl, {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.trusted_owner);
  EXPECT_TRUE(result.safe_dacl);
}

// The SDDLs below have no `O:` component, so Owner defaults to the creating
// token's default owner (which depends on the bot's elevation and policy).
// Owner trust is covered by the tests above; these cases only check the DACL.
// Each DACL also grants the current user `SYNCHRONIZE` (which `CreateFileW`
// always requests) and every policy allows it, so the test runs unelevated
// without changing any expected result.
TEST_F(FileSecurityUtilTest, InspectFileSecurityDescriptor_DaclEdgeCases) {
  std::optional<std::wstring> user = GetCurrentUser().ToSddlString();
  ASSERT_TRUE(user.has_value());
  auto with_user_synchronize = [&](std::wstring_view dacl) {
    return std::wstring(dacl) + L"(A;;0x100000;;;" + *user + L")";
  };

  SecurityDescriptorInspection result;

  // Any deny ACE makes the DACL unsafe, whether it targets a broad principal,
  // a narrower one such as `LOCAL SERVICE`, or only applies to children.
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"deny_world",
      with_user_synchronize(L"D:P(D;;0x2;;;WD)(A;;GA;;;SY)(A;;GA;;;BA)"),
      {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"deny_local_service",
      with_user_synchronize(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(D;;GA;;;LS)"),
      {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"deny_users_inherit_only",
      with_user_synchronize(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(D;OICIIO;GA;;;BU)"),
      {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);

  // A NULL DACL grants Everyone full access.
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"null_dacl", L"D:NO_ACCESS_CONTROL",
                          {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);

  // An inherit-only `CREATOR OWNER` ACE is only ignored when allowed.
  const std::wstring creator_owner_sddl =
      with_user_synchronize(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;OICIIO;GA;;;CO)");
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"creator_owner_allowed", creator_owner_sddl,
                          {.allow_inherit_only_creator_owner = true,
                           .allowed_non_admin_mask = SYNCHRONIZE},
                          &result));
  EXPECT_TRUE(result.safe_dacl);
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"creator_owner_denied", creator_owner_sddl,
                          {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);

  // Non-admin read access is only accepted within `allowed_non_admin_mask`.
  const std::wstring user_read_sddl =
      with_user_synchronize(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;FR;;;AU)");
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"user_read_allowed", user_read_sddl,
      {.allowed_non_admin_mask = FILE_GENERIC_READ | SYNCHRONIZE}, &result));
  EXPECT_TRUE(result.safe_dacl);
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"user_read_denied", user_read_sddl,
                          {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);

  // `TrustedInstaller` is only treated as trusted when allowed.
  const std::wstring trusted_installer_sddl = with_user_synchronize(
      L"D:P(A;;GA;;;SY)(A;;GA;;;BA)"
      L"(A;;GA;;;S-1-5-80-956008885-3418522649-1831038044-1853292631-"
      L"2271478464)");
  ASSERT_NO_FATAL_FAILURE(InspectNewDirectory(
      L"ti_allowed", trusted_installer_sddl,
      {.allow_trusted_installer = true, .allowed_non_admin_mask = SYNCHRONIZE},
      &result));
  EXPECT_TRUE(result.safe_dacl);
  ASSERT_NO_FATAL_FAILURE(
      InspectNewDirectory(L"ti_denied", trusted_installer_sddl,
                          {.allowed_non_admin_mask = SYNCHRONIZE}, &result));
  EXPECT_FALSE(result.safe_dacl);
}

TEST_F(FileSecurityUtilTest,
       OpenEntryNoFollow_OpensDeniedEntryWithThreadScopedPrivileges) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  const bool backup_before = IsPrivilegeEnabledOnProcess(SE_BACKUP_NAME);
  const bool restore_before = IsPrivilegeEnabledOnProcess(SE_RESTORE_NAME);

  base::FilePath denied_dir = temp_dir_.GetPath().Append(L"denied_dir");
  ASSERT_TRUE(base::CreateDirectory(denied_dir));
  const base::win::SecurityDescriptor target_sd =
      SdFromSddl(kInheritableAdminOnlySddl);

  // Restore an admin-owned SD on failure so `ScopedTempDir` can clean up.
  base::ScopedClosureRunner restore_sd(base::BindLambdaForTesting([&] {
    base::expected<base::win::ScopedHandle, DWORD> h = OpenEntryNoFollow(
        denied_dir, WRITE_DAC | WRITE_OWNER | FILE_READ_ATTRIBUTES, kShareAll,
        PrivilegedRetry::kBackupRestore);
    if (h.has_value()) {
      WriteNonPropagatingSecurityDescriptor(h->Get(), target_sd);
    }
  }));

  ASSERT_NO_FATAL_FAILURE(ApplyDenyAdminSd(denied_dir, *untrusted_owner));

  constexpr DWORD kAccess = WRITE_DAC | WRITE_OWNER | READ_CONTROL;

  // Without the privileged retry, the DACL denies access.
  base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
      denied_dir, kAccess, FILE_SHARE_READ, PrivilegedRetry::kDisabled);
  ASSERT_FALSE(handle.has_value());
  EXPECT_EQ(handle.error(), static_cast<DWORD>(ERROR_ACCESS_DENIED));

  handle = OpenEntryNoFollow(denied_dir, kAccess, FILE_SHARE_READ,
                             PrivilegedRetry::kBackupRestore);
  ASSERT_TRUE(handle.has_value()) << handle.error();
  EXPECT_FALSE(IsCurrentThreadImpersonating());
  EXPECT_EQ(IsPrivilegeEnabledOnProcess(SE_BACKUP_NAME), backup_before);
  EXPECT_EQ(IsPrivilegeEnabledOnProcess(SE_RESTORE_NAME), restore_before);

  ASSERT_TRUE(WriteNonPropagatingSecurityDescriptor(handle->Get(), target_sd));
  handle->Close();
  ASSERT_NO_FATAL_FAILURE(ExpectOwnerAndProtectedDacl(
      denied_dir,
      base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators)));
}

TEST_F(FileSecurityUtilTest,
       OpenEntryNoFollow_SkipsPrivilegedRetryWhenImpersonating) {
  if (IsCurrentUserTrustedPrincipal()) {
    GTEST_SKIP() << "Requires a TokenUser other than SY/BA.";
  }
  std::optional<std::wstring> user = GetCurrentUser().ToSddlString();
  ASSERT_TRUE(user.has_value());

  // A directory owned by the current user whose DACL denies it `WRITE_OWNER`
  // (and grants nothing to `BA`), so opening it for `WRITE_OWNER` fails with
  // `ERROR_ACCESS_DENIED` elevated or not, while the owner can still delete it.
  const std::wstring sddl =
      L"O:" + *user + L"D:P(D;;WO;;;" + *user + L")(A;;GA;;;" + *user + L")";
  std::optional<base::win::SecurityDescriptor::SelfRelative> sd =
      SdFromSddl(sddl).ToSelfRelative();
  ASSERT_TRUE(sd.has_value());
  SECURITY_ATTRIBUTES sa = {sizeof(sa), sd->get(), FALSE};
  base::FilePath dir = temp_dir_.GetPath().Append(L"no_write_owner");
  ASSERT_TRUE(::CreateDirectoryW(dir.value().c_str(), &sa));

  base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
      dir, WRITE_OWNER, FILE_SHARE_READ, PrivilegedRetry::kDisabled);
  ASSERT_FALSE(handle.has_value());
  ASSERT_EQ(handle.error(), static_cast<DWORD>(ERROR_ACCESS_DENIED));

  ASSERT_TRUE(::ImpersonateSelf(SecurityImpersonation));
  // Revert even if an assertion fails so the shared test thread does not stay
  // impersonating.
  base::ScopedClosureRunner revert(
      base::BindLambdaForTesting([] { CHECK(::RevertToSelf()); }));

  handle = OpenEntryNoFollow(dir, WRITE_OWNER, FILE_SHARE_READ,
                             PrivilegedRetry::kBackupRestore);
  ASSERT_FALSE(handle.has_value());
  EXPECT_EQ(handle.error(), static_cast<DWORD>(ERROR_ACCESS_DENIED));
  // The caller's impersonation is left in place rather than replaced and
  // reverted.
  EXPECT_TRUE(IsCurrentThreadImpersonating());
}

TEST_F(FileSecurityUtilTest, OpenEntryNoFollow_OpensFinalReparsePointItself) {
  base::FilePath real_dir = temp_dir_.GetPath().Append(L"real_dir");
  ASSERT_TRUE(base::CreateDirectory(real_dir));
  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_dir");
  ASSERT_TRUE(base::CreateDirectory(junction_dir));
  std::optional<base::test::FilePathReparsePoint> rp =
      base::test::FilePathReparsePoint::Create(junction_dir, real_dir);
  ASSERT_TRUE(rp.has_value());

  base::expected<base::win::ScopedHandle, DWORD> handle =
      OpenEntryNoFollow(junction_dir, FILE_READ_ATTRIBUTES, kShareAll,
                        PrivilegedRetry::kDisabled);
  ASSERT_TRUE(handle.has_value()) << handle.error();

  BY_HANDLE_FILE_INFORMATION info = {};
  ASSERT_TRUE(::GetFileInformationByHandle(handle->Get(), &info));
  EXPECT_NE(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT, 0u);
  EXPECT_EQ(GetCanonicalPath(handle->Get()),
            canonical_temp_.Append(L"junction_dir"));
}

TEST_F(FileSecurityUtilTest, OpenEntryNoFollow_OpensTrailingDotName) {
  // A name with a trailing dot, which Win32 path normalization would strip.
  const base::FilePath trailing_dot = temp_dir_.GetPath().Append(L"file.");
  std::optional<base::FilePath> long_trailing_dot =
      ToExtendedLengthPath(trailing_dot);
  ASSERT_TRUE(long_trailing_dot.has_value());
  // `ScopedTempDir` cannot remove a trailing-dot name.
  base::ScopedClosureRunner remove_trailing_dot(base::BindLambdaForTesting(
      [&] { ::DeleteFileW(long_trailing_dot->value().c_str()); }));
  {
    base::win::ScopedHandle h(
        ::CreateFileW(long_trailing_dot->value().c_str(), GENERIC_WRITE, 0,
                      nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(h.is_valid());
  }
  ASSERT_FALSE(base::PathExists(temp_dir_.GetPath().Append(L"file")));

  base::expected<base::win::ScopedHandle, DWORD> handle =
      OpenEntryNoFollow(trailing_dot, FILE_READ_ATTRIBUTES, kShareAll,
                        PrivilegedRetry::kDisabled);
  ASSERT_TRUE(handle.has_value()) << handle.error();
  EXPECT_EQ(GetCanonicalPath(handle->Get()), canonical_temp_.Append(L"file."));
}

TEST_F(FileSecurityUtilTest, OpenEntryNoFollow_OpensPathLongerThanMaxPath) {
  std::optional<base::FilePath> long_dir =
      ToExtendedLengthPath(temp_dir_.GetPath());
  ASSERT_TRUE(long_dir.has_value());
  std::vector<base::FilePath> created_dirs;
  // `ScopedTempDir` may not be able to remove paths longer than `MAX_PATH`.
  base::ScopedClosureRunner remove_long_dirs(base::BindLambdaForTesting([&] {
    for (const base::FilePath& dir : base::Reversed(created_dirs)) {
      ::RemoveDirectoryW(dir.value().c_str());
    }
  }));

  const std::wstring segment(60, L'd');
  base::FilePath relative;
  for (int i = 0; i < 5; ++i) {
    *long_dir = long_dir->Append(segment);
    relative = relative.Append(segment);
    ASSERT_TRUE(::CreateDirectoryW(long_dir->value().c_str(), nullptr));
    created_dirs.push_back(*long_dir);
  }
  const base::FilePath long_path = temp_dir_.GetPath().Append(relative);
  ASSERT_GT(long_path.value().size(), static_cast<size_t>(MAX_PATH));

  base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
      long_path, FILE_READ_ATTRIBUTES, kShareAll, PrivilegedRetry::kDisabled);
  ASSERT_TRUE(handle.has_value()) << handle.error();
  EXPECT_EQ(GetCanonicalPath(handle->Get()), canonical_temp_.Append(relative));
}

TEST_F(FileSecurityUtilTest, OpenEntryNoFollow_ReportsErrors) {
  base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
      temp_dir_.GetPath().Append(L"missing"), FILE_READ_ATTRIBUTES, kShareAll,
      PrivilegedRetry::kDisabled);
  ASSERT_FALSE(handle.has_value());
  EXPECT_EQ(handle.error(), static_cast<DWORD>(ERROR_FILE_NOT_FOUND));

  handle = OpenEntryNoFollow(base::FilePath(L"relative"), FILE_READ_ATTRIBUTES,
                             kShareAll, PrivilegedRetry::kDisabled);
  ASSERT_FALSE(handle.has_value());
  EXPECT_EQ(handle.error(), static_cast<DWORD>(ERROR_INVALID_NAME));

  // Non-filesystem `\\?\` paths are rejected before any open is attempted.
  handle =
      OpenEntryNoFollow(base::FilePath(L"\\\\?\\pipe\\x"), FILE_READ_ATTRIBUTES,
                        kShareAll, PrivilegedRetry::kDisabled);
  ASSERT_FALSE(handle.has_value());
  EXPECT_EQ(handle.error(), static_cast<DWORD>(ERROR_INVALID_NAME));
}

TEST_F(FileSecurityUtilTest,
       WriteNonPropagatingSecurityDescriptor_ProtectsDaclAndSkipsChildren) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir");
  ASSERT_TRUE(base::CreateDirectory(dir));
  base::FilePath child = dir.Append(L"child.json");
  ASSERT_TRUE(base::WriteFile(child, "data"));
  std::optional<std::wstring> child_sddl_before = GetOwnerAndDaclSddl(child);
  ASSERT_TRUE(child_sddl_before.has_value());

  // An unprotected, inheritable SD is still written as protected, and is not
  // propagated to the existing child. The inheritable `LOCAL SERVICE` ACE is
  // not present on the temp directory, so a propagating write would change
  // the child's DACL.
  base::win::ScopedHandle handle(
      ::CreateFileW(dir.value().c_str(), WRITE_DAC | WRITE_OWNER, kShareAll,
                    nullptr, OPEN_EXISTING, kNoFollowFlags, nullptr));
  ASSERT_TRUE(handle.is_valid());
  ASSERT_TRUE(WriteNonPropagatingSecurityDescriptor(
      handle.Get(),
      SdFromSddl(UserOwnedSddl(L"", L"OICI") + L"(A;OICI;FR;;;LS)")));
  handle.Close();

  ASSERT_NO_FATAL_FAILURE(ExpectOwnerAndProtectedDacl(dir, GetCurrentUser()));
  EXPECT_EQ(GetOwnerAndDaclSddl(child), child_sddl_before);
}

TEST_F(FileSecurityUtilTest,
       WriteNonPropagatingSecurityDescriptor_RejectsIncompleteSd) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir");
  ASSERT_TRUE(base::CreateDirectory(dir));
  std::optional<std::wstring> sddl_before = GetOwnerAndDaclSddl(dir);
  ASSERT_TRUE(sddl_before.has_value());

  base::win::ScopedHandle handle(::CreateFileW(
      dir.value().c_str(), WRITE_DAC | WRITE_OWNER | READ_CONTROL, kShareAll,
      nullptr, OPEN_EXISTING, kNoFollowFlags, nullptr));
  ASSERT_TRUE(handle.is_valid());

  // No DACL (which would grant Everyone full access). The SDs below use the
  // current user as owner/group, which the token can always assign, so the
  // writes could only fail because of the fail-closed checks, even unelevated.
  std::optional<std::wstring> user = GetCurrentUser().ToSddlString();
  ASSERT_TRUE(user.has_value());
  const std::wstring owner = L"O:" + *user;
  const std::wstring group = L"G:" + *user;
  const std::wstring dacl = L"D:(A;;GA;;;" + *user + L")";
  EXPECT_FALSE(WriteNonPropagatingSecurityDescriptor(
      handle.Get(), SdFromSddl(owner + group)));
  // NULL DACL (which would grant Everyone full access).
  const std::wstring null_dacl_sddl = owner + group + L"D:NO_ACCESS_CONTROL";
  EXPECT_FALSE(WriteNonPropagatingSecurityDescriptor(
      handle.Get(), SdFromSddl(null_dacl_sddl)));
  // Missing owner or group.
  EXPECT_FALSE(WriteNonPropagatingSecurityDescriptor(handle.Get(),
                                                     SdFromSddl(group + dacl)));
  EXPECT_FALSE(WriteNonPropagatingSecurityDescriptor(handle.Get(),
                                                     SdFromSddl(owner + dacl)));
  handle.Close();

  EXPECT_EQ(GetOwnerAndDaclSddl(dir), sddl_before);

  // `ApplyNonPropagatingSdToVerifiedPath()` fails closed the same way.
  EXPECT_FALSE(ApplyNonPropagatingSdToVerifiedPath(
      dir, canonical_temp_.Append(L"dir"), kShareAll,
      SdFromSddl(null_dacl_sddl)));
  EXPECT_EQ(GetOwnerAndDaclSddl(dir), sddl_before);
}

TEST_F(FileSecurityUtilTest, ApplyNonPropagatingSdToVerifiedPath_AppliesSd) {
  base::FilePath dir = temp_dir_.GetPath().Append(L"dir");
  ASSERT_TRUE(base::CreateDirectory(dir));

  EXPECT_TRUE(ApplyNonPropagatingSdToVerifiedPath(
      dir, canonical_temp_.Append(L"dir"), kShareAll,
      SdFromSddl(UserOwnedSddl(L"P", L"OICI"))));
  ASSERT_NO_FATAL_FAILURE(ExpectOwnerAndProtectedDacl(dir, GetCurrentUser()));
}

TEST_F(FileSecurityUtilTest,
       ApplyNonPropagatingSdToVerifiedPath_RejectsRedirectedAndLinkedEntries) {
  // The SD is owned by the current user so that, if a rejection check were
  // missing, the write would succeed (and be detected below) even unelevated.
  const base::win::SecurityDescriptor sd = SdFromSddl(UserOwnedSddl(L"P", L""));

  base::FilePath real_dir = temp_dir_.GetPath().Append(L"real_dir");
  ASSERT_TRUE(base::CreateDirectory(real_dir));
  base::FilePath real_file = real_dir.Append(L"file.json");
  ASSERT_TRUE(base::WriteFile(real_file, "data"));

  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_dir");
  ASSERT_TRUE(base::CreateDirectory(junction_dir));
  std::optional<base::test::FilePathReparsePoint> rp =
      base::test::FilePathReparsePoint::Create(junction_dir, real_dir);
  ASSERT_TRUE(rp.has_value());

  std::optional<std::wstring> real_file_sddl = GetOwnerAndDaclSddl(real_file);
  ASSERT_TRUE(real_file_sddl.has_value());
  std::optional<std::wstring> junction_sddl = GetOwnerAndDaclSddl(junction_dir);
  ASSERT_TRUE(junction_sddl.has_value());

  // A parent component that is a junction resolves outside the expected tree.
  EXPECT_FALSE(ApplyNonPropagatingSdToVerifiedPath(
      junction_dir.Append(L"file.json"),
      canonical_temp_.Append(L"junction_dir").Append(L"file.json"), kShareAll,
      sd));
  EXPECT_EQ(GetOwnerAndDaclSddl(real_file), real_file_sddl);

  // The reparse point itself is rejected.
  EXPECT_FALSE(ApplyNonPropagatingSdToVerifiedPath(
      junction_dir, canonical_temp_.Append(L"junction_dir"), kShareAll, sd));
  EXPECT_EQ(GetOwnerAndDaclSddl(junction_dir), junction_sddl);

  // A hard link to a file elsewhere is rejected even though its canonical path
  // matches.
  base::FilePath hard_link = temp_dir_.GetPath().Append(L"link.json");
  ASSERT_TRUE(base::CreateWinHardLink(hard_link, real_file));
  EXPECT_FALSE(ApplyNonPropagatingSdToVerifiedPath(
      hard_link, canonical_temp_.Append(L"link.json"), kShareAll, sd));
  EXPECT_EQ(GetOwnerAndDaclSddl(real_file), real_file_sddl);
}

}  // namespace remoting
