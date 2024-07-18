// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_io_request.h"

#import "base/files/file_path.h"
#import "base/files/scoped_temp_dir.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/session_internal_util.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Constants used by unit tests.
const base::FilePath::CharType kFilename[] = FILE_PATH_LITERAL("file");
const base::FilePath::CharType kDirname1[] = FILE_PATH_LITERAL("dir1");
const base::FilePath::CharType kDirname2[] = FILE_PATH_LITERAL("dir2");

}  // namespace

using SessionIORequestTest = PlatformTest;

// Tests that WriteDataIORequest writes the data to the path when executed.
TEST_F(SessionIORequestTest, WriteDataIORequest) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that the destination file does not exist yet.
  const base::FilePath filename = root.Append(kFilename);
  ASSERT_FALSE(ios::sessions::FileExists(filename));

  // Create the WriteDataIORequest and check that the file has not yet
  // been created.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ios::sessions::WriteDataIORequest request(filename, data);
  EXPECT_FALSE(ios::sessions::FileExists(filename));

  // Check that executing the request write the data to disk, and that
  // the file contains the correct data.
  request.Execute();

  EXPECT_TRUE(ios::sessions::FileExists(filename));
  EXPECT_NSEQ(ios::sessions::ReadFile(filename), data);
}

// Tests that WriteProtoIORequest writes the proto to the path when executed.
TEST_F(SessionIORequestTest, WriteProtoIORequest) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that the destination file does not exist yet.
  const base::FilePath filename = root.Append(kFilename);
  ASSERT_FALSE(ios::sessions::FileExists(filename));

  // Create a protobuf message that is not empty.
  auto proto = std::make_unique<ios::proto::WebStateListStorage>();
  proto->set_active_index(-1);

  // Create the WriteDataIORequest and check that the file has not yet
  // been created.
  ios::sessions::WriteProtoIORequest request(filename, std::move(proto));
  EXPECT_FALSE(ios::sessions::FileExists(filename));

  // Check that executing the request write the data to disk, and that
  // the file is not empty.
  request.Execute();

  EXPECT_TRUE(ios::sessions::FileExists(filename));
  EXPECT_NSNE(ios::sessions::ReadFile(filename), nil);
}

// Tests that CopyPathIORequest correctly copy the source to destination when
// executed (recursively).
TEST_F(SessionIORequestTest, CopyPathIORequest) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that the destination file does not exist yet.
  const base::FilePath from_dir = root.Append(kDirname1);
  const base::FilePath dest_dir = root.Append(kDirname2);
  ASSERT_FALSE(ios::sessions::DirectoryExists(from_dir));
  ASSERT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Create the source directory with one file in it.
  const base::FilePath from_file = from_dir.Append(kFilename);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_file, data));
  ASSERT_TRUE(ios::sessions::DirectoryExists(from_dir));
  ASSERT_TRUE(ios::sessions::FileExists(from_file));

  // Create the CopyPathIORequest and check that the path has not been
  // copied yet.
  ios::sessions::CopyPathIORequest request(from_dir, dest_dir);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dest_dir));

  // Check that executing the request copy the path recursively.
  request.Execute();

  const base::FilePath dest_file = dest_dir.Append(kFilename);
  EXPECT_TRUE(ios::sessions::FileExists(dest_file));
  EXPECT_NSEQ(ios::sessions::ReadFile(dest_file), data);
  EXPECT_TRUE(ios::sessions::DirectoryExists(dest_dir));
}

// Tests that DeletePathIORequest correctly delete the path (recursively).
TEST_F(SessionIORequestTest, DeletePathIORequest) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that the destination file does not exist yet.
  const base::FilePath dir_name = root.Append(kDirname1);
  const base::FilePath filename = dir_name.Append(kFilename);

  // Create the directory with one file in it.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(filename, data));
  ASSERT_TRUE(ios::sessions::DirectoryExists(dir_name));
  ASSERT_TRUE(ios::sessions::FileExists(filename));

  // Create the DeletePathIORequest and check that the path has not been
  // deleted yet.
  ios::sessions::DeletePathIORequest request(dir_name);
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir_name));

  // Check that executing the request delete the path recursively.
  request.Execute();

  EXPECT_FALSE(ios::sessions::DirectoryExists(dir_name));
  EXPECT_FALSE(ios::sessions::FileExists(filename));
}

// Tests that ExecuteIORequests execute the requests the order they have
// been pushed to the vector.
TEST_F(SessionIORequestTest, ExecuteIORequests) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from_dir = root.Append(kDirname1);
  const base::FilePath dest_dir = root.Append(kDirname2);

  const base::FilePath from_file = from_dir.Append(kFilename);
  const base::FilePath dest_file = dest_dir.Append(kFilename);

  // Create a directory with a file in it.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_file, data));
  ASSERT_TRUE(ios::sessions::DirectoryExists(from_dir));
  ASSERT_TRUE(ios::sessions::FileExists(from_file));

  // Create an IORequestList that request a copy of from_dir to dest_dir,
  // and then delete dest_dir.
  ios::sessions::IORequestList requests;
  requests.push_back(
      std::make_unique<ios::sessions::CopyPathIORequest>(from_dir, dest_dir));
  requests.push_back(
      std::make_unique<ios::sessions::DeletePathIORequest>(from_dir));

  // Check that the source is copied and then deleted when executing the
  // list of requests.
  ExecuteIORequests(std::move(requests));

  ASSERT_TRUE(ios::sessions::DirectoryExists(dest_dir));
  ASSERT_NSEQ(ios::sessions::ReadFile(dest_file), data);
  ASSERT_TRUE(ios::sessions::FileExists(dest_file));

  ASSERT_FALSE(ios::sessions::DirectoryExists(from_dir));
  ASSERT_FALSE(ios::sessions::FileExists(from_file));
}
