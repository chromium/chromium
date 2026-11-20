// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/secure_delete.h"

#include <windows.h>

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
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "base/win/sid.h"
#include "remoting/host/win/file_security_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

constexpr DWORD kShareAll =
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

constexpr wchar_t kAdminOnlySddl[] = L"O:BAG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)";
constexpr wchar_t kDenyAdminSddl[] = L"D:P(D;;GA;;;BA)";

base::win::SecurityDescriptor SdFromSddl(base::wcstring_view sddl) {
  std::optional<base::win::SecurityDescriptor> sd =
      base::win::SecurityDescriptor::FromSddl(sddl);
  CHECK(sd.has_value());
  return std::move(*sd);
}

// Returns a protected SD owned by the current process user granting Generic All
// to `SY`, `BA`, and the current user. Setting the token user as owner needs no
// privilege, so tests using this run unelevated.
base::win::SecurityDescriptor UserOwnedSd() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  CHECK(token.has_value());
  std::optional<std::wstring> user = token->User().ToSddlString();
  CHECK(user.has_value());
  return SdFromSddl(L"O:" + *user + L"G:" + *user +
                    L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;" + *user + L")");
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

// Sets `owner` as Owner and a protected DACL denying `BA` all access on `path`.
void ApplyDenyAdminSd(const base::FilePath& path, const base::win::Sid& owner) {
  base::win::SecurityDescriptor sd = SdFromSddl(kDenyAdminSddl);
  sd.set_owner(owner);
  ASSERT_TRUE(sd.WriteToFile(
      path, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION));
}

}  // namespace

class SecureDeleteTest : public testing::Test {
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

  base::ScopedTempDir temp_dir_;
  base::FilePath canonical_temp_;
  const base::win::SecurityDescriptor user_owned_sd_ = UserOwnedSd();
  const base::win::SecurityDescriptor admin_only_sd_ =
      SdFromSddl(kAdminOnlySddl);
};

TEST_F(SecureDeleteTest, RemovesRegularAndReadOnlyFiles) {
  base::FilePath plain_file = temp_dir_.GetPath().Append(L"plain.json");
  ASSERT_TRUE(base::WriteFile(plain_file, "delete_me"));
  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      plain_file, canonical_temp_.Append(L"plain.json"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(plain_file));

  base::FilePath readonly_file = temp_dir_.GetPath().Append(L"readonly.txt");
  ASSERT_TRUE(base::WriteFile(readonly_file, "delete_me"));
  ASSERT_TRUE(
      ::SetFileAttributesW(readonly_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));
  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      readonly_file, canonical_temp_.Append(L"readonly.txt"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(readonly_file));
}

TEST_F(SecureDeleteTest, RemovesDirectorySubtreeAndNestedJunction) {
  base::FilePath outside_target = temp_dir_.GetPath().Append(L"outside_target");
  ASSERT_TRUE(base::CreateDirectory(outside_target));
  base::FilePath precious_file = outside_target.Append(L"precious.txt");
  ASSERT_TRUE(base::WriteFile(precious_file, "keep"));

  base::FilePath untrusted_subdir = temp_dir_.GetPath().Append(L"untrusted");
  ASSERT_TRUE(base::CreateDirectory(untrusted_subdir));
  base::FilePath readonly_file = untrusted_subdir.Append(L"readonly.txt");
  ASSERT_TRUE(base::WriteFile(readonly_file, "delete_me"));
  ASSERT_TRUE(
      ::SetFileAttributesW(readonly_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));
  base::FilePath nested_dir = untrusted_subdir.Append(L"sub");
  ASSERT_TRUE(base::CreateDirectory(nested_dir));
  ASSERT_TRUE(base::WriteFile(nested_dir.Append(L"nested.txt"), "delete_me"));

  base::FilePath nested_junction = untrusted_subdir.Append(L"nested_junction");
  ASSERT_TRUE(base::CreateDirectory(nested_junction));
  std::optional<base::test::FilePathReparsePoint> nested_rp =
      base::test::FilePathReparsePoint::Create(nested_junction, outside_target);
  ASSERT_TRUE(nested_rp.has_value());

  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      untrusted_subdir, canonical_temp_.Append(L"untrusted"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(untrusted_subdir));
  EXPECT_TRUE(base::DirectoryExists(outside_target));
  EXPECT_TRUE(base::PathExists(precious_file));
}

TEST_F(SecureDeleteTest, RemovesDeniedTreeWithPrivilegedRetry) {
  std::optional<base::win::Sid> untrusted_owner = GetUntrustedOwnerForTesting();
  if (!untrusted_owner.has_value()) {
    GTEST_SKIP() << "Requires an elevated, non-BA/SY TokenUser.";
  }

  base::FilePath untrusted_subdir = temp_dir_.GetPath().Append(L"untrusted");
  ASSERT_TRUE(base::CreateDirectory(untrusted_subdir));
  base::FilePath readonly_file = untrusted_subdir.Append(L"readonly.txt");
  ASSERT_TRUE(base::WriteFile(readonly_file, "delete_me"));
  ASSERT_TRUE(
      ::SetFileAttributesW(readonly_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));

  // Best-effort cleanup so a failed assertion below does not leak a tree whose
  // DACL denies `BA`.
  base::ScopedClosureRunner cleanup(base::BindLambdaForTesting([&] {
    DeleteUntrustedEntrySafely(
        untrusted_subdir, canonical_temp_.Append(L"untrusted"), admin_only_sd_);
  }));

  ASSERT_NO_FATAL_FAILURE(ApplyDenyAdminSd(untrusted_subdir, *untrusted_owner));

  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      untrusted_subdir, canonical_temp_.Append(L"untrusted"), admin_only_sd_));
  EXPECT_FALSE(base::PathExists(untrusted_subdir));
}

TEST_F(SecureDeleteTest, FailsClosedOnLockedFile) {
  // Handle omitting `FILE_SHARE_DELETE`: both opens fail with a sharing
  // violation.
  base::FilePath locked_file = temp_dir_.GetPath().Append(L"locked.json");
  ASSERT_TRUE(base::WriteFile(locked_file, "locked"));
  base::File no_share_delete_handle(
      locked_file, base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_TRUE(no_share_delete_handle.IsValid());

  EXPECT_FALSE(DeleteUntrustedEntrySafely(
      locked_file, canonical_temp_.Append(L"locked.json"), user_owned_sd_));
  EXPECT_TRUE(base::PathExists(locked_file));
  no_share_delete_handle.Close();

  // Write handle with `FILE_SHARE_DELETE`: the initial `FILE_SHARE_READ` open
  // hits `ERROR_SHARING_VIOLATION`, the share-all retry succeeds, and the
  // non-reparse check rejects deletion.
  base::FilePath write_held_file = temp_dir_.GetPath().Append(L"writable.json");
  ASSERT_TRUE(base::WriteFile(write_held_file, "writable"));
  base::File write_handle(write_held_file,
                          base::File::FLAG_OPEN | base::File::FLAG_WRITE |
                              base::File::FLAG_WIN_SHARE_DELETE);
  ASSERT_TRUE(write_handle.IsValid());

  EXPECT_FALSE(DeleteUntrustedEntrySafely(
      write_held_file, canonical_temp_.Append(L"writable.json"),
      user_owned_sd_));
  EXPECT_TRUE(base::PathExists(write_held_file));
  write_handle.Close();
}

TEST_F(SecureDeleteTest, TopLevelReparsePoint) {
  base::FilePath outside_target = temp_dir_.GetPath().Append(L"outside_target");
  ASSERT_TRUE(base::CreateDirectory(outside_target));
  base::FilePath precious_file = outside_target.Append(L"precious.txt");
  ASSERT_TRUE(base::WriteFile(precious_file, "keep"));

  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_dir");
  ASSERT_TRUE(base::CreateDirectory(junction_dir));
  std::optional<base::test::FilePathReparsePoint> rp =
      base::test::FilePathReparsePoint::Create(junction_dir, outside_target);
  ASSERT_TRUE(rp.has_value());

  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      junction_dir, canonical_temp_.Append(L"junction_dir"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(junction_dir));
  EXPECT_TRUE(base::DirectoryExists(outside_target));
  EXPECT_TRUE(base::PathExists(precious_file));
}

TEST_F(SecureDeleteTest, HandlesUnusualNamesAndHardLinks) {
  base::FilePath outside_file = temp_dir_.GetPath().Append(L"outside.txt");
  ASSERT_TRUE(base::WriteFile(outside_file, "keep"));

  base::FilePath untrusted = temp_dir_.GetPath().Append(L"untrusted");
  ASSERT_TRUE(base::CreateDirectory(untrusted));

  // A name with a trailing dot, which Win32 path normalization would strip.
  std::optional<base::FilePath> trailing_dot =
      ToExtendedLengthPath(untrusted.Append(L"host.json."));
  ASSERT_TRUE(trailing_dot.has_value());
  // `ScopedTempDir` cannot remove a trailing-dot name if the test fails early.
  base::ScopedClosureRunner remove_trailing_dot(base::BindLambdaForTesting(
      [&] { ::DeleteFileW(trailing_dot->value().c_str()); }));
  {
    base::win::ScopedHandle h(
        ::CreateFileW(trailing_dot->value().c_str(), GENERIC_WRITE, 0, nullptr,
                      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    ASSERT_TRUE(h.is_valid());
  }

  // A nested directory chain longer than `MAX_PATH`.
  std::optional<base::FilePath> long_dir = ToExtendedLengthPath(untrusted);
  ASSERT_TRUE(long_dir.has_value());
  const std::wstring segment(60, L'd');
  for (int i = 0; i < 6; ++i) {
    *long_dir = long_dir->Append(segment);
    ASSERT_TRUE(::CreateDirectoryW(long_dir->value().c_str(), nullptr));
  }
  ASSERT_GT(long_dir->value().size(), static_cast<size_t>(MAX_PATH));

  // A hard link to a read-only file outside the tree.
  ASSERT_TRUE(
      base::CreateWinHardLink(untrusted.Append(L"link.txt"), outside_file));
  ASSERT_TRUE(
      ::SetFileAttributesW(outside_file.value().c_str(),
                           FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_READONLY));

  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      untrusted, canonical_temp_.Append(L"untrusted"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(untrusted));

  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(outside_file, &contents));
  EXPECT_EQ(contents, "keep");
  DWORD attrs = ::GetFileAttributesW(outside_file.value().c_str());
  ASSERT_NE(attrs, INVALID_FILE_ATTRIBUTES);
  EXPECT_NE(attrs & FILE_ATTRIBUTE_READONLY, 0u);
  ASSERT_TRUE(::SetFileAttributesW(outside_file.value().c_str(),
                                   FILE_ATTRIBUTE_NORMAL));
}

TEST_F(SecureDeleteTest, EnforcesDepthLimit) {
  // Boundary: a tree whose deepest directory is at depth
  // `kMaxDeleteRecursionDepth` (32) must be deleted successfully.
  base::FilePath at_limit_root = temp_dir_.GetPath().Append(L"d32");
  base::FilePath at_limit_dir = at_limit_root;
  ASSERT_TRUE(base::CreateDirectory(at_limit_dir));
  for (int i = 0; i < 32; ++i) {
    at_limit_dir = at_limit_dir.Append(L"d");
    ASSERT_TRUE(base::CreateDirectory(at_limit_dir));
  }
  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      at_limit_root, canonical_temp_.Append(L"d32"), user_owned_sd_));
  EXPECT_FALSE(base::PathExists(at_limit_root));

  // One level deeper (depth 33) exceeds the limit and must fail closed.
  base::FilePath over_limit_root = temp_dir_.GetPath().Append(L"d33");
  base::FilePath over_limit_dir = over_limit_root;
  ASSERT_TRUE(base::CreateDirectory(over_limit_dir));
  for (int i = 0; i < 33; ++i) {
    over_limit_dir = over_limit_dir.Append(L"d");
    ASSERT_TRUE(base::CreateDirectory(over_limit_dir));
  }
  EXPECT_FALSE(DeleteUntrustedEntrySafely(
      over_limit_root, canonical_temp_.Append(L"d33"), user_owned_sd_));
  EXPECT_TRUE(base::DirectoryExists(over_limit_dir));
}

TEST_F(SecureDeleteTest, RejectsCanonicalPathMismatchAndRedirectedParent) {
  base::FilePath outside_target = temp_dir_.GetPath().Append(L"outside_target");
  ASSERT_TRUE(base::CreateDirectory(outside_target));
  base::FilePath precious_file = outside_target.Append(L"precious.txt");
  ASSERT_TRUE(base::WriteFile(precious_file, "keep"));

  base::FilePath junction_dir = temp_dir_.GetPath().Append(L"junction_dir");
  ASSERT_TRUE(base::CreateDirectory(junction_dir));
  std::optional<base::test::FilePathReparsePoint> rp =
      base::test::FilePathReparsePoint::Create(junction_dir, outside_target);
  ASSERT_TRUE(rp.has_value());

  // A file reached through a parent junction resolves outside the expected
  // canonical path and must not be deleted.
  EXPECT_FALSE(DeleteUntrustedEntrySafely(
      junction_dir.Append(L"precious.txt"),
      canonical_temp_.Append(L"junction_dir").Append(L"precious.txt"),
      user_owned_sd_));
  std::string contents;
  EXPECT_TRUE(base::ReadFileToString(precious_file, &contents));
  EXPECT_EQ(contents, "keep");

  // Direct mismatch between the opened entry and `expected_canonical_path`.
  base::FilePath actual_file = temp_dir_.GetPath().Append(L"actual.txt");
  ASSERT_TRUE(base::WriteFile(actual_file, "keep"));
  EXPECT_FALSE(DeleteUntrustedEntrySafely(
      actual_file, canonical_temp_.Append(L"other.txt"), user_owned_sd_));
  EXPECT_TRUE(base::PathExists(actual_file));
}

TEST_F(SecureDeleteTest, MissingPathReturnsTrue) {
  base::FilePath missing = temp_dir_.GetPath().Append(L"does_not_exist.txt");
  EXPECT_TRUE(DeleteUntrustedEntrySafely(
      missing, canonical_temp_.Append(L"does_not_exist.txt"), user_owned_sd_));
}

TEST_F(SecureDeleteTest, RejectsPathsWithoutExtendedLengthForm) {
  EXPECT_FALSE(DeleteUntrustedEntrySafely(base::FilePath(L"relative"),
                                          canonical_temp_.Append(L"relative"),
                                          user_owned_sd_));
}

}  // namespace remoting::installer
