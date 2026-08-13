// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"

#import "base/files/file_path.h"
#import "base/path_service.h"
#import "base/strings/string_number_conversions.h"
#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/download/model/auto_deletion/proto/scheduled_file.pb.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"
#import "testing/platform_test.h"

namespace {

// Test file name.
const base::FilePath::CharType kTestFileName[] =
    FILE_PATH_LITERAL("test_filepath.txt");

}  // namespace

namespace auto_deletion {

class ScheduledFileTest : public PlatformTest {};

// Tests that a ScheduledFile can be serialized into a proto object.
TEST_F(ScheduledFileTest, SerializeScheduledFile) {
  base::FilePath path = base::FilePath(kTestFileName);
  const auto_deletion::ScheduledFile file(path, base::Time::Now());
  base::Time time = file.download_time();
  int64_t epoch = time.ToDeltaSinceWindowsEpoch().InMicroseconds();

  const auto_deletion::proto::ScheduledFile proto = file.Serialize();

  EXPECT_EQ(proto.path(), path.AsUTF8Unsafe());
  EXPECT_EQ(proto.download_timestamp(), epoch);
}

// Tests that a ScheduledFile can be constructed from a serialized string.
TEST_F(ScheduledFileTest, DeserializeScheduledFile) {
  base::FilePath path = base::FilePath(kTestFileName);
  auto file =
      std::make_unique<auto_deletion::ScheduledFile>(path, base::Time::Now());
  const auto_deletion::proto::ScheduledFile proto = file->Serialize();

  std::optional<ScheduledFile> deserializedFile =
      ScheduledFile::Deserialize(proto);

  ASSERT_TRUE(deserializedFile.has_value());
  EXPECT_EQ(*file, *deserializedFile);
}

// Tests that null is returned if the proto contains an empty path.
TEST_F(ScheduledFileTest, DeserializeScheduledFileIsNullWhenPathIsEmpty) {
  auto_deletion::proto::ScheduledFile proto;

  std::optional<ScheduledFile> deserializedFile =
      ScheduledFile::Deserialize(proto);

  EXPECT_FALSE(deserializedFile.has_value());
}

}  // namespace auto_deletion
