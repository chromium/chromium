// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/config_dir_entry_security.h"

#include <windows.h>

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
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
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/secure_delete.h"
#include "remoting/host/win/file_security_util.h"
#include "remoting/host/win/security_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

// A directory owned by `BA` whose DACL only grants `SY` and `BA` access.
constexpr wchar_t kAdminOwnedDirSddl[] =
    L"O:BAG:BAD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)";

// Protected SDDL matching `host_unprivileged.json` (`SY`/`BA` GA, `AU` GR).
constexpr wchar_t kUnprivilegedConfigSddl[] =
    L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)";

constexpr wchar_t kDenyAdminSddl[] = L"D:P(D;;GA;;;BA)";

constexpr DWORD kShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
constexpr DWORD kShareNoDelete = FILE_SHARE_READ | FILE_SHARE_WRITE;

constexpr DWORD kPinnedDirAccess = READ_CONTROL | WRITE_DAC | WRITE_OWNER |
                                   FILE_READ_ATTRIBUTES | FILE_LIST_DIRECTORY;

base::win::SecurityDescriptor SdFromSddl(base::wcstring_view sddl) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(sddl);
  CHECK(sd.has_value());
  return std::move(*sd);
}

// Returns the current process user if the test is running elevated as a
// non-BA/SY TokenUser, so that an untrusted owner can be simulated.
std::optional<base::win::Sid> GetUntrustedOwnerForTesting() {
  if (!base::IsCurrentProcessElevated()) {
    return std::nullopt;
  }
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token.has_value()) {
    return std::nullopt;
  }
  base::win::Sid user = token->User();
  if (user == base::win::Sid(base::win::WellKnownSid::kLocalSystem) ||
      user == base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators)) {
    return std::nullopt;
  }
  return user;
}

void CreateDirectoryWithSddl(const base::FilePath& path,
                             base::wcstring_view sddl) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(sddl);
  ASSERT_TRUE(sd.has_value());
  std::optional<base::win::SecurityDescriptor::SelfRelative> self_relative =
      sd->ToSelfRelative();
  ASSERT_TRUE(self_relative.has_value());
  SECURITY_ATTRIBUTES sa = {sizeof(sa), self_relative->get(), FALSE};
  ASSERT_TRUE(::CreateDirectoryW(path.value().c_str(), &sa));
}

void CreateFileWithSddl(const base::FilePath& path, base::wcstring_view sddl) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(sddl);
  ASSERT_TRUE(sd.has_value());
  std::optional<base::win::SecurityDescriptor::SelfRelative> self_relative =
      sd->ToSelfRelative();
  ASSERT_TRUE(self_relative.has_value());
  SECURITY_ATTRIBUTES sa = {sizeof(sa), self_relative->get(), FALSE};
  base::win::ScopedHandle h(::CreateFileW(path.value().c_str(), GENERIC_WRITE,
                                          0, &sa, CREATE_NEW,
                                          FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(h.is_valid());
}

void SetOwner(const base::FilePath& path, const base::win::Sid& owner) {
  base::win::SecurityDescriptor sd;
  sd.set_owner(owner);
  ASSERT_TRUE(sd.WriteToFile(path, OWNER_SECURITY_INFORMATION));
}

void ApplyDenyAdminSd(const base::FilePath& path, const base::win::Sid& owner) {
  base::win::SecurityDescriptor sd = SdFromSddl(kDenyAdminSddl);
  sd.set_owner(owner);
  ASSERT_TRUE(sd.WriteToFile(
      path, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION));
}

std::optional<std::wstring> GetOwnerAndDaclSddl(const base::FilePath& path) {
  constexpr SECURITY_INFORMATION kOwnerAndDacl =
      OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION;
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(path, kOwnerAndDacl);
  if (!sd.has_value()) {
    return std::nullopt;
  }
  return sd->ToSddl(kOwnerAndDacl);
}

std::optional<bool> DaclGrantsDirectAccessToWellKnownSid(
    const base::FilePath& path,
    base::win::WellKnownSid well_known_sid) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  if (!sd.has_value() || !sd->dacl().has_value() || sd->dacl()->is_null()) {
    return std::nullopt;
  }
  PACL dacl = sd->dacl()->get();

  ACL_SIZE_INFORMATION acl_info = {};
  if (!::GetAclInformation(dacl, &acl_info, sizeof(acl_info),
                           AclSizeInformation)) {
    return std::nullopt;
  }

  const base::win::Sid target_sid(well_known_sid);
  for (DWORD i = 0; i < acl_info.AceCount; ++i) {
    void* raw_ace = nullptr;
    if (!::GetAce(dacl, i, &raw_ace)) {
      return std::nullopt;
    }
    const auto* header = static_cast<const ACE_HEADER*>(raw_ace);
    if (header->AceType != ACCESS_ALLOWED_ACE_TYPE ||
        (header->AceFlags & INHERIT_ONLY_ACE) != 0) {
      continue;
    }
    const auto* allow_ace = static_cast<const ACCESS_ALLOWED_ACE*>(raw_ace);
    PSID sid =
        const_cast<SID*>(reinterpret_cast<const SID*>(&allow_ace->SidStart));
    if (target_sid.Equal(sid) && allow_ace->Mask != 0) {
      return true;
    }
  }
  return false;
}

// Queries `TokenPrivileges` on the primary process token via
// `base::win::AccessToken::FromCurrentProcess()` (avoiding `PrivilegeCheck`,
// which rejects primary tokens). Returns `std::nullopt` if the process token
// cannot be queried, or whether `privilege_name` is present and enabled.
std::optional<bool> IsPrivilegeEnabledOnProcess(const wchar_t* privilege_name) {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token.has_value()) {
    return std::nullopt;
  }
  for (const base::win::AccessToken::Privilege& privilege :
       token->Privileges()) {
    if (privilege.GetName() == privilege_name) {
      return privilege.IsEnabled();
    }
  }
  return false;
}

void ExpectAdminOwnedAndProtectedDacl(const base::FilePath& path) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromFile(
          path, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION);
  ASSERT_TRUE(sd.has_value());
  ASSERT_TRUE(sd->owner().has_value());
  ASSERT_TRUE(sd->dacl().has_value());
  EXPECT_FALSE(sd->dacl()->is_null());
  EXPECT_EQ(*sd->owner(),
            base::win::Sid(base::win::WellKnownSid::kBuiltinAdministrators));
  EXPECT_TRUE(sd->dacl_protected());
}

void ExpectNoThreadImpersonationToken() {
  const bool has_thread_token =
      base::win::AccessToken::FromCurrentThread().has_value();
  const DWORD thread_token_error = ::GetLastError();
  EXPECT_FALSE(has_thread_token);
  EXPECT_EQ(thread_token_error, static_cast<DWORD>(ERROR_NO_TOKEN));
}

// Pins `config_dir` without `FILE_SHARE_DELETE` (matching the caller
// precondition of `PurgeAndHardenConfigDirEntries()`) and resolves its
// canonical path.
void PinAndResolveConfigDir(const base::FilePath& config_dir,
                            base::win::ScopedHandle* out_pin_handle,
                            base::FilePath* out_canonical_path) {
  base::expected<base::win::ScopedHandle, DWORD> opened = OpenEntryNoFollow(
      config_dir, kPinnedDirAccess, kShareNoDelete, PrivilegedRetry::kDisabled);
  ASSERT_TRUE(opened.has_value());
  std::optional<base::FilePath> canonical = GetCanonicalPath(opened->Get());
  ASSERT_TRUE(canonical.has_value());
  *out_pin_handle = std::move(opened).value();
  *out_canonical_path = *std::move(canonical);
}

}  // namespace

class ConfigDirEntrySecurityTest : public testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::ScopedTempDir temp_dir_;
  const base::win::SecurityDescriptor admin_only_sd_ =
      SdFromSddl(kAdminOnlyProtectedSddl);
};

TEST_F(ConfigDirEntrySecurityTest, FailsOnRelativeMissingOrNonDirectoryPath) {
  // Relative path fails `ToExtendedLengthPath()`.
  EXPECT_FALSE(
      PurgeAndHardenConfigDirEntries(base::FilePath(L"relative_config"),
                                     temp_dir_.GetPath().Append(L"relative"))
          .has_value());

  // Missing directory fails `base::FileEnumerator`.
  base::FilePath missing_dir = temp_dir_.GetPath().Append(L"missing_config");
  EXPECT_FALSE(
      PurgeAndHardenConfigDirEntries(missing_dir, missing_dir).has_value());

  // Regular file (not a directory) fails `base::FileEnumerator`.
  base::FilePath regular_file = temp_dir_.GetPath().Append(L"not_a_dir.txt");
  ASSERT_TRUE(base::WriteFile(regular_file, "data"));
  EXPECT_FALSE(
      PurgeAndHardenConfigDirEntries(regular_file, regular_file).has_value());
}

// Note: Windows CQ bots (`win-rel`) execute Swarming tasks with UAC disabled
// (`EnableLUA=0`), so `base::IsCurrentProcessElevated()` is true on CQ.
TEST_F(ConfigDirEntrySecurityTest, EmptyDirectoryReturnsZeroRetained) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"empty_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);
}

TEST_F(ConfigDirEntrySecurityTest,
       PreservesAlreadySafeFilesIdempotentlyEvenWithExclusiveReadHandles) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"safe_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  // A `BA`-owned `host.json` whose unprotected DACL only grants `SY`/`BA`
  // access must be left untouched.
  base::FilePath host_json = config_dir.Append(kHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithSddl(host_json, L"O:BAG:BAD:(A;;GA;;;BA)(A;;GA;;;SY)"));
  std::optional<std::wstring> host_sddl_before = GetOwnerAndDaclSddl(host_json);
  ASSERT_TRUE(host_sddl_before.has_value());

  // A `BA`-owned `host_unprivileged.json` that already carries `AU` read-only
  // access is preserved idempotently without rewriting its security descriptor.
  base::FilePath unpriv_json =
      config_dir.Append(kUnprivilegedHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithSddl(unpriv_json, kUnprivilegedConfigSddl));
  std::optional<std::wstring> unpriv_sddl_before =
      GetOwnerAndDaclSddl(unpriv_json);
  ASSERT_TRUE(unpriv_sddl_before.has_value());

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  // Any local user can open `host_unprivileged.json` with `GENERIC_READ` and
  // `dwShareMode = 0`. Already-safe files must not be reopened with
  // `FILE_READ_DATA`, so such handles cannot DoS setup.
  base::win::ScopedHandle exclusive_unpriv_reader(
      ::CreateFileW(unpriv_json.value().c_str(), GENERIC_READ,
                    /*dwShareMode=*/0, nullptr, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(exclusive_unpriv_reader.is_valid());
  base::win::ScopedHandle exclusive_host_reader(
      ::CreateFileW(host_json.value().c_str(), GENERIC_READ, /*dwShareMode=*/0,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  ASSERT_TRUE(exclusive_host_reader.is_valid());

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 2u);

  EXPECT_TRUE(base::PathExists(host_json));
  EXPECT_EQ(GetOwnerAndDaclSddl(host_json), host_sddl_before);
  EXPECT_TRUE(base::PathExists(unpriv_json));
  EXPECT_EQ(GetOwnerAndDaclSddl(unpriv_json), unpriv_sddl_before);
}

TEST_F(ConfigDirEntrySecurityTest,
       PurgesUntrustedOwnerFilesIncludingUnusualNames) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"untrusted_files");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  // A file with a strict admin/system DACL whose Owner is an individual admin
  // user SID rather than `BA` or `SY` is untrusted.
  base::FilePath admin_user_owned_file = config_dir.Append(L"planted.json");
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(admin_user_owned_file,
                                             L"D:P(A;;GA;;;BA)(A;;GA;;;SY)"));
  ASSERT_NO_FATAL_FAILURE(SetOwner(admin_user_owned_file, *untrusted_owner));

  // Untrusted files whose extended-length names end with a trailing dot or
  // space (which Win32 path normalization would strip if enumerated without
  // `\\?\`).
  std::optional<base::FilePath> trailing_dot_file =
      ToExtendedLengthPath(config_dir.Append(L"planted.json."));
  std::optional<base::FilePath> trailing_space_file =
      ToExtendedLengthPath(config_dir.Append(L"planted.json "));
  ASSERT_TRUE(trailing_dot_file.has_value());
  ASSERT_TRUE(trailing_space_file.has_value());
  base::ScopedClosureRunner cleanup_unusual_names(
      base::BindLambdaForTesting([&] {
        ::DeleteFileW(trailing_dot_file->value().c_str());
        ::DeleteFileW(trailing_space_file->value().c_str());
      }));
  for (const base::FilePath& unusual :
       {*trailing_dot_file, *trailing_space_file}) {
    ASSERT_NO_FATAL_FAILURE(
        CreateFileWithSddl(unusual, L"D:P(A;;GA;;;BA)(A;;GA;;;SY)"));
    ASSERT_NO_FATAL_FAILURE(SetOwner(unusual, *untrusted_owner));
  }

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);

  EXPECT_FALSE(base::PathExists(admin_user_owned_file));
  EXPECT_EQ(::GetFileAttributesW(trailing_dot_file->value().c_str()),
            INVALID_FILE_ATTRIBUTES);
  EXPECT_EQ(::GetFileAttributesW(trailing_space_file->value().c_str()),
            INVALID_FILE_ATTRIBUTES);
}

TEST_F(ConfigDirEntrySecurityTest,
       TightensReadPermissiveDaclOnAdminOwnedFiles) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"permissive_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  // `host.json` with read-only-permissive non-admin access (e.g. inherited from
  // a legacy MSI install) is tightened in place to admin/system-only.
  base::FilePath host_json = config_dir.Append(kHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      host_json, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)"));

  // `host_unprivileged.json` with read-only non-admin access already satisfies
  // policy and is retained untouched.
  base::FilePath unpriv_json =
      config_dir.Append(kUnprivilegedHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      unpriv_json, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)"));
  std::optional<std::wstring> unpriv_sddl_before =
      GetOwnerAndDaclSddl(unpriv_json);
  ASSERT_TRUE(unpriv_sddl_before.has_value());

  // A `BA`-owned read-permissive file whose DACL grants `BA` only `GR` (no
  // `WRITE_OWNER`) forces `PrivilegedRetry::kBackupRestore` during tightening.
  base::FilePath backup_retry_file = config_dir.Append(L"backup_retry.json");
  base::ScopedClosureRunner cleanup_backup_retry(
      base::BindLambdaForTesting([&] {
        DeleteUntrustedEntrySafely(
            backup_retry_file,
            canonical_config_dir.Append(L"backup_retry.json"), admin_only_sd_);
      }));
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      backup_retry_file, L"O:BAG:BAD:P(A;;GR;;;BA)(A;;GA;;;SY)(A;;GR;;;BU)"));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 3u);

  for (const base::FilePath& tightened : {host_json, backup_retry_file}) {
    EXPECT_TRUE(base::PathExists(tightened));
    ASSERT_NO_FATAL_FAILURE(ExpectAdminOwnedAndProtectedDacl(tightened));

    std::optional<bool> grants_bu = DaclGrantsDirectAccessToWellKnownSid(
        tightened, base::win::WellKnownSid::kBuiltinUsers);
    ASSERT_TRUE(grants_bu.has_value());
    EXPECT_FALSE(*grants_bu);

    std::optional<bool> grants_au = DaclGrantsDirectAccessToWellKnownSid(
        tightened, base::win::WellKnownSid::kAuthenticatedUser);
    ASSERT_TRUE(grants_au.has_value());
    EXPECT_FALSE(*grants_au);

    base::expected<base::win::ScopedHandle, DWORD> handle = OpenEntryNoFollow(
        tightened, READ_CONTROL, kShareAll, PrivilegedRetry::kDisabled);
    ASSERT_TRUE(handle.has_value());
    std::optional<SecurityDescriptorInspection> inspection =
        InspectFileSecurityDescriptor(handle->Get(),
                                      {.allowed_non_admin_mask = 0});
    ASSERT_TRUE(inspection.has_value());
    EXPECT_TRUE(inspection->trusted_owner);
    EXPECT_TRUE(inspection->safe_dacl);
  }

  EXPECT_TRUE(base::PathExists(unpriv_json));
  EXPECT_EQ(GetOwnerAndDaclSddl(unpriv_json), unpriv_sddl_before);
  ExpectNoThreadImpersonationToken();
}

TEST_F(ConfigDirEntrySecurityTest,
       RemovesWriteAndMetadataPermissiveAdminOwnedFiles) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir =
      temp_dir_.GetPath().Append(L"write_permissive_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  // Admin-owned files granting non-admins write or ACL-editing access have
  // untrusted contents and may have stale metadata handles, so they must be
  // deleted rather than tightened in place.
  base::FilePath writable_host = config_dir.Append(kHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      writable_host, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)"));

  base::FilePath writable_unpriv =
      config_dir.Append(kUnprivilegedHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      writable_unpriv, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;BU)"));

  base::FilePath metadata_writable = config_dir.Append(L"write_dac.json");
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      metadata_writable, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;WD;;;BU)"));

  // A `BA`-owned file missing `BA` read access also fails safety checks and is
  // removed via privileged retry inside `DeleteUntrustedEntrySafely()`.
  base::FilePath missing_admin_read = config_dir.Append(L"rc_only_ba.json");
  base::ScopedClosureRunner cleanup_rc_only(base::BindLambdaForTesting([&] {
    DeleteUntrustedEntrySafely(missing_admin_read,
                               canonical_config_dir.Append(L"rc_only_ba.json"),
                               admin_only_sd_);
  }));
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      missing_admin_read, L"O:BAG:BAD:P(A;;RC;;;BA)(A;;GA;;;SY)"));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);

  EXPECT_FALSE(base::PathExists(writable_host));
  EXPECT_FALSE(base::PathExists(writable_unpriv));
  EXPECT_FALSE(base::PathExists(metadata_writable));
  EXPECT_FALSE(base::PathExists(missing_admin_read));
}

TEST_F(ConfigDirEntrySecurityTest,
       FailsWhenTighteningFileHeldOpenWithWriteOrDeleteAccess) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"open_handle_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::FilePath host_json = config_dir.Append(kHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      host_json, L"O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)"));
  std::optional<std::wstring> permissive_sddl = GetOwnerAndDaclSddl(host_json);
  ASSERT_TRUE(permissive_sddl.has_value());

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  // A pre-opened data-write handle (even sharing everything) fails closed
  // before the DACL is tightened.
  {
    base::win::ScopedHandle write_handle(
        ::CreateFileW(host_json.value().c_str(), GENERIC_WRITE, kShareAll,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(write_handle.is_valid());
    EXPECT_FALSE(
        PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir)
            .has_value());
    EXPECT_EQ(GetOwnerAndDaclSddl(host_json), permissive_sddl);
  }

  // A pre-opened `DELETE` handle also fails closed.
  {
    base::win::ScopedHandle delete_handle(
        ::CreateFileW(host_json.value().c_str(), DELETE, kShareAll, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(delete_handle.is_valid());
    EXPECT_FALSE(
        PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir)
            .has_value());
    EXPECT_EQ(GetOwnerAndDaclSddl(host_json), permissive_sddl);
  }

  // Concurrent read-only handles (`FILE_SHARE_READ`) are permitted, and
  // tightening succeeds once data-write/delete handles are closed.
  {
    base::win::ScopedHandle read_handle(
        ::CreateFileW(host_json.value().c_str(), GENERIC_READ, FILE_SHARE_READ,
                      nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(read_handle.is_valid());
    std::optional<size_t> retained =
        PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
    ASSERT_TRUE(retained.has_value());
    EXPECT_EQ(*retained, 1u);
    ASSERT_NO_FATAL_FAILURE(ExpectAdminOwnedAndProtectedDacl(host_json));
  }
}

TEST_F(ConfigDirEntrySecurityTest, RemovesMultiplyLinkedFilesUnconditionally) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"linked_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  // Hard links to both a permissive `BA`-owned file and an already-safe
  // `BA`-owned file outside `config_dir` are unlinked without modifying the
  // shared file objects.
  base::FilePath outside_permissive =
      temp_dir_.GetPath().Append(L"outside_permissive.json");
  ASSERT_NO_FATAL_FAILURE(CreateFileWithSddl(
      outside_permissive, L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;BU)"));
  base::FilePath outside_safe =
      temp_dir_.GetPath().Append(L"outside_safe.json");
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithSddl(outside_safe, kAdminOnlyProtectedSddl));

  base::FilePath linked_permissive = config_dir.Append(kHostConfigFileName);
  base::FilePath linked_safe =
      config_dir.Append(kUnprivilegedHostConfigFileName);
  ASSERT_TRUE(base::CreateWinHardLink(linked_permissive, outside_permissive));
  ASSERT_TRUE(base::CreateWinHardLink(linked_safe, outside_safe));

  std::optional<std::wstring> permissive_sddl_before =
      GetOwnerAndDaclSddl(outside_permissive);
  std::optional<std::wstring> safe_sddl_before =
      GetOwnerAndDaclSddl(outside_safe);
  ASSERT_TRUE(permissive_sddl_before.has_value());
  ASSERT_TRUE(safe_sddl_before.has_value());

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);

  EXPECT_FALSE(base::PathExists(linked_permissive));
  EXPECT_FALSE(base::PathExists(linked_safe));
  EXPECT_TRUE(base::PathExists(outside_permissive));
  EXPECT_TRUE(base::PathExists(outside_safe));
  EXPECT_EQ(GetOwnerAndDaclSddl(outside_permissive), permissive_sddl_before);
  EXPECT_EQ(GetOwnerAndDaclSddl(outside_safe), safe_sddl_before);
}

TEST_F(ConfigDirEntrySecurityTest,
       RemovesAccessDeniedEntriesAndRestoresPrivileges) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  const std::optional<bool> take_ownership_before =
      IsPrivilegeEnabledOnProcess(SE_TAKE_OWNERSHIP_NAME);
  const std::optional<bool> backup_before =
      IsPrivilegeEnabledOnProcess(SE_BACKUP_NAME);
  const std::optional<bool> restore_before =
      IsPrivilegeEnabledOnProcess(SE_RESTORE_NAME);
  ASSERT_TRUE(take_ownership_before.has_value());
  ASSERT_TRUE(backup_before.has_value());
  ASSERT_TRUE(restore_before.has_value());

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"denied_entries");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  base::FilePath denied_file = config_dir.Append(L"planted.json");
  base::FilePath denied_subdir = config_dir.Append(L"planted_subdir");
  // Best-effort cleanup so an early assertion failure does not leave an
  // undeletable read-only/access-denied tree behind in `%TEMP%`.
  base::ScopedClosureRunner cleanup_denied(base::BindLambdaForTesting([&] {
    DeleteUntrustedEntrySafely(denied_file,
                               canonical_config_dir.Append(L"planted.json"),
                               admin_only_sd_);
    DeleteUntrustedEntrySafely(denied_subdir,
                               canonical_config_dir.Append(L"planted_subdir"),
                               admin_only_sd_);
  }));

  ASSERT_TRUE(base::WriteFile(denied_file, "planted"));
  ASSERT_TRUE(
      ::SetFileAttributesW(denied_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));

  ASSERT_TRUE(base::CreateDirectory(denied_subdir));
  base::FilePath nested_file = denied_subdir.Append(L"nested.txt");
  ASSERT_TRUE(base::WriteFile(nested_file, "nested"));
  ASSERT_TRUE(
      ::SetFileAttributesW(nested_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));

  ASSERT_NO_FATAL_FAILURE(ApplyDenyAdminSd(denied_file, *untrusted_owner));
  ASSERT_NO_FATAL_FAILURE(ApplyDenyAdminSd(denied_subdir, *untrusted_owner));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);

  EXPECT_FALSE(base::PathExists(denied_file));
  EXPECT_FALSE(base::PathExists(denied_subdir));
  ExpectNoThreadImpersonationToken();
  EXPECT_EQ(IsPrivilegeEnabledOnProcess(SE_TAKE_OWNERSHIP_NAME),
            take_ownership_before);
  EXPECT_EQ(IsPrivilegeEnabledOnProcess(SE_BACKUP_NAME), backup_before);
  EXPECT_EQ(IsPrivilegeEnabledOnProcess(SE_RESTORE_NAME), restore_before);
}

TEST_F(ConfigDirEntrySecurityTest,
       RemovesAdminOwnedSubdirectoriesAndRetriesAfterLockedChildFailure) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"subdir_retry");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  // Even a pre-existing `BA`-owned subdirectory is removed because legitimate
  // Windows configuration directories are flat.
  base::FilePath admin_subdir = config_dir.Append(L"admin_subdir");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(admin_subdir, kAdminOwnedDirSddl));
  ASSERT_TRUE(base::WriteFile(admin_subdir.Append(L"child.txt"), "child"));

  // An untrusted subdirectory whose child is locked on the first run: Run 1
  // rewrites `untrusted_subdir`'s owner/DACL to `kAdminOnlyProtectedSddl` and
  // fails on `locked.txt`; Run 2 must still remove `untrusted_subdir` rather
  // than treating its rewritten `BA` owner/DACL as trusted.
  base::FilePath untrusted_subdir = config_dir.Append(L"untrusted_subdir");
  ASSERT_TRUE(base::CreateDirectory(untrusted_subdir));
  ASSERT_NO_FATAL_FAILURE(SetOwner(untrusted_subdir, *untrusted_owner));
  base::FilePath locked_child = untrusted_subdir.Append(L"locked.txt");
  ASSERT_TRUE(base::WriteFile(locked_child, "locked"));
  ASSERT_NO_FATAL_FAILURE(SetOwner(locked_child, *untrusted_owner));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  {
    base::File locked_handle(locked_child,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(locked_handle.IsValid());
    EXPECT_FALSE(
        PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir)
            .has_value());
  }
  ASSERT_NO_FATAL_FAILURE(ExpectAdminOwnedAndProtectedDacl(untrusted_subdir));
  EXPECT_TRUE(base::PathExists(locked_child));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);
  EXPECT_FALSE(base::PathExists(admin_subdir));
  EXPECT_FALSE(base::PathExists(untrusted_subdir));
}

TEST_F(ConfigDirEntrySecurityTest,
       PurgesUntrustedSubdirectoriesAndJunctionsSafely) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  base::FilePath outside_target = temp_dir_.GetPath().Append(L"outside_target");
  ASSERT_TRUE(base::CreateDirectory(outside_target));
  base::FilePath precious_file = outside_target.Append(L"precious.txt");
  ASSERT_TRUE(base::WriteFile(precious_file, "do_not_delete"));

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"config_with_links");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::FilePath direct_junction = config_dir.Append(L"direct_junction");
  ASSERT_TRUE(base::CreateDirectory(direct_junction));
  std::optional<base::test::FilePathReparsePoint> direct_rp =
      base::test::FilePathReparsePoint::Create(direct_junction, outside_target);
  ASSERT_TRUE(direct_rp.has_value());

  base::FilePath untrusted_subdir = config_dir.Append(L"untrusted_subdir");
  ASSERT_TRUE(base::CreateDirectory(untrusted_subdir));
  ASSERT_NO_FATAL_FAILURE(SetOwner(untrusted_subdir, *untrusted_owner));
  ASSERT_TRUE(
      base::WriteFile(untrusted_subdir.Append(L"nested.txt"), "untrusted"));

  base::FilePath nested_junction = untrusted_subdir.Append(L"nested_junction");
  ASSERT_TRUE(base::CreateDirectory(nested_junction));
  std::optional<base::test::FilePathReparsePoint> nested_rp =
      base::test::FilePathReparsePoint::Create(nested_junction, outside_target);
  ASSERT_TRUE(nested_rp.has_value());

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  std::optional<size_t> retained =
      PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir);
  ASSERT_TRUE(retained.has_value());
  EXPECT_EQ(*retained, 0u);

  EXPECT_EQ(::GetFileAttributesW(direct_junction.value().c_str()),
            INVALID_FILE_ATTRIBUTES);
  EXPECT_FALSE(base::PathExists(untrusted_subdir));
  EXPECT_TRUE(base::DirectoryExists(outside_target));
  EXPECT_TRUE(base::PathExists(precious_file));
}

TEST_F(ConfigDirEntrySecurityTest,
       FailsClosedWithoutDeletingOnCanonicalPathMismatch) {
  if (!base::IsCurrentProcessElevated()) {
    GTEST_SKIP() << "Requires an elevated token to set BA owner.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"real_config");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));
  base::FilePath host_json = config_dir.Append(kHostConfigFileName);
  ASSERT_NO_FATAL_FAILURE(
      CreateFileWithSddl(host_json, kAdminOnlyProtectedSddl));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  // Passing a mismatched expected canonical directory simulates a path
  // normalization/redirection anomaly on a non-reparse child; the call must
  // fail closed without deleting `host_json`.
  base::FilePath mismatched_canonical =
      canonical_config_dir.DirName().Append(L"other_config");
  EXPECT_FALSE(PurgeAndHardenConfigDirEntries(config_dir, mismatched_canonical)
                   .has_value());
  EXPECT_TRUE(base::PathExists(host_json));
}

TEST_F(ConfigDirEntrySecurityTest,
       FailsClosedWhenUntrustedFileLockedNoDeleteShare) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  base::FilePath config_dir = temp_dir_.GetPath().Append(L"locked_untrusted");
  ASSERT_NO_FATAL_FAILURE(
      CreateDirectoryWithSddl(config_dir, kAdminOwnedDirSddl));

  base::FilePath planted_file = config_dir.Append(kHostConfigFileName);
  ASSERT_TRUE(base::WriteFile(planted_file, "planted"));
  ASSERT_NO_FATAL_FAILURE(SetOwner(planted_file, *untrusted_owner));

  base::win::ScopedHandle pin_handle;
  base::FilePath canonical_config_dir;
  ASSERT_NO_FATAL_FAILURE(
      PinAndResolveConfigDir(config_dir, &pin_handle, &canonical_config_dir));

  base::File locked_handle(planted_file,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(locked_handle.IsValid());

  EXPECT_FALSE(PurgeAndHardenConfigDirEntries(config_dir, canonical_config_dir)
                   .has_value());

  locked_handle.Close();
}

}  // namespace remoting::installer
