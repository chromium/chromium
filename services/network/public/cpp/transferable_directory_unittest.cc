// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/transferable_directory.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/network_service_buildflags.h"
#include "services/network/public/mojom/transferable_directory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using TransferableDirectoryTest = testing::Test;

const base::FilePath kDirPath(FILE_PATH_LITERAL("/some/directory"));

TEST_F(TransferableDirectoryTest, ManipulatePathOnly) {
  // Initialize empty TransferableDirectory with path.
  TransferableDirectory dir;
  EXPECT_TRUE(dir.path().empty());
  dir = kDirPath;
  EXPECT_EQ(dir.path(), kDirPath);

  // Reassign to path.
  const base::FilePath kReassignedPath(FILE_PATH_LITERAL("/reassigned/path"));
  dir = base::FilePath(kReassignedPath);
  EXPECT_EQ(dir.path(), kReassignedPath);

  // Reassign to another TransferableDirectory.
  TransferableDirectory assigned;
  assigned = std::move(dir);
  EXPECT_EQ(assigned.path(), kReassignedPath);

  // Construct with path.
  TransferableDirectory constructed(kDirPath);
  EXPECT_EQ(constructed.path(), kDirPath);
}

TEST_F(TransferableDirectoryTest, MojoTraitsWithPath) {
  TransferableDirectory dir(kDirPath);
  TransferableDirectory roundtripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransferableDirectory>(
      dir, roundtripped));

  EXPECT_EQ(dir.path(), roundtripped.path());
}

#if BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

class TransferableDirectoryTestWithHandle : public testing::Test {
 public:
  TransferableDirectoryTestWithHandle() = default;
  ~TransferableDirectoryTestWithHandle() override = default;

  static constexpr char kFileFromParent[] = "from_parent";
  static constexpr char kFileFromChild[] = "from_child";
  static constexpr char kData[] = "yarr yee";

  void SetUp() override {
    // Create a temporary directory and publish a file into it.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(
        base::WriteFile(temp_dir_.GetPath().Append(kFileFromParent), kData));

    // Construct a TransferableDirectory from the temp dir and prepare it for
    // mounting.
    transferable_temp_dir_ = TransferableDirectory(temp_dir_.GetPath());
    EXPECT_TRUE(transferable_temp_dir_.IsOpenForTransferRequired());
    EXPECT_FALSE(transferable_temp_dir_.NeedsMount());
    transferable_temp_dir_.OpenForTransfer();
    EXPECT_TRUE(transferable_temp_dir_.NeedsMount());
  }

 protected:
  base::ScopedTempDir temp_dir_;
  TransferableDirectory transferable_temp_dir_;
};

TEST_F(TransferableDirectoryTestWithHandle, OpenMountAndUnmount) {
  {
    // Mount the directory and verify that the parent-published file can be
    // seen.
    base::ScopedClosureRunner scoped_mount(transferable_temp_dir_.Mount());
    EXPECT_NE(transferable_temp_dir_.path(), temp_dir_.GetPath());

    EXPECT_TRUE(base::PathExists(transferable_temp_dir_.path()));
    ASSERT_FALSE(transferable_temp_dir_.path().empty());
    ASSERT_TRUE(base::PathExists(transferable_temp_dir_.path()));
    EXPECT_TRUE(base::PathExists(
        transferable_temp_dir_.path().Append(kFileFromParent)));

    // Write a file into the directory, for the parent to see.
    EXPECT_FALSE(
        base::PathExists(transferable_temp_dir_.path().Append(kFileFromChild)));
    ASSERT_TRUE(
        base::WriteFile(temp_dir_.GetPath().Append(kFileFromChild), kData));
    EXPECT_TRUE(
        base::PathExists(transferable_temp_dir_.path().Append(kFileFromChild)));
  }

  // |scoped_mount| is no longer in scope, so verify that the directory was
  // unmounted.
  EXPECT_FALSE(base::PathExists(transferable_temp_dir_.path()));

  // The child-published file should still be available in the original tempdir.
  EXPECT_TRUE(base::PathExists(temp_dir_.GetPath().Append(kFileFromChild)));
}

TEST_F(TransferableDirectoryTestWithHandle, SupportsMoveAsHandle) {
  // Mount the directory and verify that the published file exists.
  TransferableDirectory moved_temp_dir = std::move(transferable_temp_dir_);
  auto scoped_mount = moved_temp_dir.Mount();
  EXPECT_TRUE(base::PathExists(moved_temp_dir.path()));
  ASSERT_FALSE(moved_temp_dir.path().empty());
  EXPECT_TRUE(base::PathExists(moved_temp_dir.path().Append(kFileFromParent)));
}

TEST_F(TransferableDirectoryTestWithHandle, MojoTraitsWithHandle) {
  // Verify that the handle is preserved when transmitted using
  // mojo::TransferableDirectory.
  TransferableDirectory roundtripped;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::TransferableDirectory>(
      transferable_temp_dir_, roundtripped));

  auto scoped_mount = roundtripped.Mount();
  ASSERT_TRUE(base::PathExists(roundtripped.path()));
  EXPECT_TRUE(base::PathExists(roundtripped.path().Append(kFileFromParent)));
}

#else

TEST_F(TransferableDirectoryTest, OpenAndMountNotSupportedForPlatform) {
  TransferableDirectory dir;
  dir = kDirPath;
  EXPECT_EQ(dir.path(), kDirPath);

  EXPECT_FALSE(dir.IsOpenForTransferRequired());
  EXPECT_FALSE(dir.NeedsMount());
}

#endif  // BUILDFLAG(IS_DIRECTORY_TRANSFER_REQUIRED)

}  // namespace
}  // namespace network
