// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/secure_file_reader.h"

#include <sys/stat.h>
#include <unistd.h>

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::HasSubstr;
using ::testing::Optional;

constexpr size_t kMaxSizeBytes = 1024;
constexpr char kContent[] = "content";

// Matches an error Loggable whose message contains `substring`.
auto ErrorContains(const std::string& substring) {
  return base::test::ErrorIs(::testing::Property(
      "ToString", &Loggable::ToString, HasSubstr(substring)));
}

}  // namespace

class SecureFileReaderTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    directory_ = temp_dir_.GetPath().Append("dir");
    file_ = directory_.Append("file");
    current_uid_ = geteuid();
  }

 protected:
  void CreateDirectory(int mode = 0700) {
    ASSERT_TRUE(base::CreateDirectory(directory_));
    ASSERT_TRUE(base::SetPosixFilePermissions(directory_, mode));
  }

  void CreateFile(const std::string& content = kContent,
                  int file_mode = 0600,
                  int directory_mode = 0700) {
    ASSERT_NO_FATAL_FAILURE(CreateDirectory(directory_mode));
    ASSERT_TRUE(base::WriteFile(file_, content));
    ASSERT_TRUE(base::SetPosixFilePermissions(file_, file_mode));
  }

  base::expected<std::optional<std::string>, Loggable> Read() {
    return ReadSecureFile(file_, current_uid_, kMaxSizeBytes);
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath directory_;
  base::FilePath file_;
  uid_t current_uid_ = 0;
};

TEST_F(SecureFileReaderTest, ReadsSecureFile) {
  ASSERT_NO_FATAL_FAILURE(CreateFile());
  EXPECT_THAT(Read(), base::test::ValueIs(Optional(std::string(kContent))));
}

TEST_F(SecureFileReaderTest, ReadsEmptyFile) {
  ASSERT_NO_FATAL_FAILURE(CreateFile(""));
  EXPECT_THAT(Read(), base::test::ValueIs(Optional(std::string())));
}

TEST_F(SecureFileReaderTest, AcceptsReadableByOthers) {
  ASSERT_NO_FATAL_FAILURE(
      CreateFile(kContent, /*file_mode=*/0644, /*directory_mode=*/0755));
  EXPECT_THAT(Read(), base::test::ValueIs(Optional(std::string(kContent))));
}

TEST_F(SecureFileReaderTest, AcceptsFileAtSizeLimit) {
  const std::string kMaxContent(kMaxSizeBytes, 'a');
  ASSERT_NO_FATAL_FAILURE(CreateFile(kMaxContent));
  EXPECT_THAT(Read(), base::test::ValueIs(Optional(kMaxContent)));
}

TEST_F(SecureFileReaderTest, ReturnsNulloptIfDirectoryIsMissing) {
  EXPECT_THAT(Read(), base::test::ValueIs(std::nullopt));
}

TEST_F(SecureFileReaderTest, ReturnsNulloptIfFileIsMissing) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory());
  EXPECT_THAT(Read(), base::test::ValueIs(std::nullopt));
}

TEST_F(SecureFileReaderTest, RejectsInsecureDirectoryEvenIfFileIsMissing) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory(0770));
  EXPECT_THAT(Read(), ErrorContains("writable by group or others"));
}

TEST_F(SecureFileReaderTest, RejectsSymlinkFile) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory());
  base::FilePath target = temp_dir_.GetPath().Append("target");
  ASSERT_TRUE(base::WriteFile(target, kContent));
  ASSERT_TRUE(base::CreateSymbolicLink(target, file_));
  EXPECT_THAT(Read(), ErrorContains("symbolic link"));
}

TEST_F(SecureFileReaderTest, RejectsSymlinkDirectory) {
  base::FilePath real_directory = temp_dir_.GetPath().Append("real");
  ASSERT_TRUE(base::CreateDirectory(real_directory));
  ASSERT_TRUE(base::WriteFile(real_directory.Append("file"), kContent));
  ASSERT_TRUE(base::CreateSymbolicLink(real_directory, directory_));
  EXPECT_THAT(Read(), ErrorContains("symbolic link"));
}

TEST_F(SecureFileReaderTest, RejectsDanglingSymlinkDirectory) {
  ASSERT_TRUE(base::CreateSymbolicLink(
      temp_dir_.GetPath().Append("nonexistent"), directory_));
  EXPECT_THAT(Read(), ErrorContains(directory_.value()));
}

TEST_F(SecureFileReaderTest, RejectsRegularFileAsDirectory) {
  ASSERT_TRUE(base::WriteFile(directory_, kContent));
  EXPECT_THAT(Read(), ErrorContains(directory_.value()));
}

TEST_F(SecureFileReaderTest, RejectsDirectoryAsFile) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory());
  ASSERT_TRUE(base::CreateDirectory(file_));
  EXPECT_THAT(Read(), ErrorContains("not a regular file"));
}

TEST_F(SecureFileReaderTest, RejectsFifoWithoutBlocking) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory());
  ASSERT_EQ(mkfifo(file_.value().c_str(), 0600), 0);
  EXPECT_THAT(Read(), ErrorContains("not a regular file"));
}

TEST_F(SecureFileReaderTest, RejectsWrongOwner) {
  // Note that this fails the owner check of the parent directory, since the
  // file can't be owned by a different user than its directory without root.
  ASSERT_NO_FATAL_FAILURE(CreateFile());
  EXPECT_THAT(ReadSecureFile(file_, current_uid_ + 1, kMaxSizeBytes),
              ErrorContains("'" + directory_.value() + "' has owner UID"));
}

TEST_F(SecureFileReaderTest, RejectsGroupOrWorldWritableFile) {
  for (int mode : {0620, 0602}) {
    SCOPED_TRACE(mode);
    ASSERT_NO_FATAL_FAILURE(CreateFile(kContent, mode));
    EXPECT_THAT(Read(), ErrorContains("'" + file_.value() +
                                      "' is writable by group or others"));
  }
}

TEST_F(SecureFileReaderTest, RejectsGroupOrWorldWritableDirectory) {
  for (int mode : {0770, 0707}) {
    SCOPED_TRACE(mode);
    ASSERT_NO_FATAL_FAILURE(CreateFile(kContent, /*file_mode=*/0600, mode));
    EXPECT_THAT(Read(), ErrorContains("'" + directory_.value() +
                                      "' is writable by group or others"));
  }
}

TEST_F(SecureFileReaderTest, RejectsOversizedFile) {
  ASSERT_NO_FATAL_FAILURE(CreateFile(std::string(kMaxSizeBytes + 1, 'a')));
  EXPECT_THAT(Read(), ErrorContains("exceeds the size limit"));
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryAcceptsSecureDirectory) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory(0755));
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_),
              base::test::HasValue());
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryRejectsMissingDirectory) {
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_),
              ErrorContains(directory_.value()));
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryRejectsSymlink) {
  base::FilePath real_directory = temp_dir_.GetPath().Append("real");
  ASSERT_TRUE(base::CreateDirectory(real_directory));
  ASSERT_TRUE(base::CreateSymbolicLink(real_directory, directory_));
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_),
              ErrorContains("symbolic link"));
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryRejectsRegularFile) {
  ASSERT_TRUE(base::WriteFile(directory_, kContent));
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_),
              ErrorContains(directory_.value()));
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryRejectsWrongOwner) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory());
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_ + 1),
              ErrorContains("has owner UID"));
}

TEST_F(SecureFileReaderTest, CheckSecureDirectoryRejectsWritableByOthers) {
  ASSERT_NO_FATAL_FAILURE(CreateDirectory(0777));
  EXPECT_THAT(CheckSecureDirectory(directory_, current_uid_),
              ErrorContains("writable by group or others"));
}

}  // namespace remoting
