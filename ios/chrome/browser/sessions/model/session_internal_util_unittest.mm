// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_internal_util.h"

#import "base/files/file_enumerator.h"
#import "base/files/scoped_temp_dir.h"
#import "base/time/time.h"
#import "ios/chrome/browser/sessions/model/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/model/proto_util.h"
#import "ios/chrome/browser/sessions/model/session_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using SessionInternalUtilTest = PlatformTest;

namespace {

// Constants used to construct filenames.
const base::FilePath::CharType kFilename[] = FILE_PATH_LITERAL("file");
const base::FilePath::CharType kDirname1[] = FILE_PATH_LITERAL("dir1");
const base::FilePath::CharType kDirname2[] = FILE_PATH_LITERAL("dir2");
const base::FilePath::CharType kDirname3[] = FILE_PATH_LITERAL("dir3");
const base::FilePath::CharType kFromName[] = FILE_PATH_LITERAL("from");
const base::FilePath::CharType kDestName[] = FILE_PATH_LITERAL("dest");

// A sub-class of google::protobuf::MessageLite that cannot be serialized.
//
// Note: sub-classing google::protobuf::MessageLite is not supported, so
// this may break at any point. If this break, we may consider removing
// this class and the test using it (the class exists to get as much
// coverage as possible for `WriteProto` function).
//
// The implementation is broken, and the only goal is to have the call to
// `SerializeToArray()` in `WriteProto` to fail. This is achieved by using
// a mutable state that allow returning a size of serialized data that is
// increasing each time `ByteSizeLong()` is called (thus resulting in an
// allocation that is considered too small by `SerializeToArray()`).
//
// All the other methods are overridden to be no-op.
class UnserializableMessage : public google::protobuf::MessageLite {
 public:
  // google::protobuf::MessageLite
  std::string GetTypeName() const override { return "UnserializableMessage"; }

  MessageLite* New(google::protobuf::Arena* arena) const override {
    return nullptr;
  }

  void Clear() override {}

  bool IsInitialized() const override { return true; }

  void CheckTypeAndMergeFrom(const MessageLite& other) override {}

  size_t ByteSizeLong() const override {
    return ++call_count_ * sizeof(double);
  }

  int GetCachedSize() const override {
    return static_cast<int>(ByteSizeLong());
  }

  uint8_t* _InternalSerialize(
      uint8_t* ptr,
      google::protobuf::io::EpsCopyOutputStream* stream) const override {
    return ptr;
  }

 private:
  // Record how many time `ByteSizeLong()` is called, allowing to return a
  // different size for the serialized data each time it is called, which
  // eventually leads to a failure of `SerializeToArray()`.
  mutable size_t call_count_ = 1;
};

// Creates a SessionWindowIOS* with fake data.
SessionWindowIOS* CreateSessionWindowIOS() {
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.stableIdentifier = [[NSUUID UUID] UUIDString];
  session_storage.uniqueIdentifier = web::WebStateID::NewUnique();
  session_storage.creationTime = base::Time::Now();
  session_storage.lastActiveTime = session_storage.creationTime;
  session_storage.lastCommittedItemIndex = -1;
  session_storage.itemStorages = @[];

  return [[SessionWindowIOS alloc] initWithSessions:@[ session_storage ]
                                          tabGroups:@[]
                                      selectedIndex:0];
}

// Returns the list of item at `path`.
std::set<base::FilePath> DirectoryContent(const base::FilePath& path) {
  using FileEnumerator = base::FileEnumerator;
  constexpr int all_items =
      FileEnumerator::FileType::FILES | FileEnumerator::FileType::DIRECTORIES;

  std::set<base::FilePath> result;
  FileEnumerator e(path, false, all_items);
  for (base::FilePath name = e.Next(); !name.empty(); name = e.Next()) {
    result.insert(e.GetInfo().GetName());
  }

  return result;
}

// Compares the content of two path and check they are identical (recursively).
bool PathAreIdentical(const base::FilePath& lhs, const base::FilePath& rhs) {
  // If both path are file, check whether they have the same content.
  if (ios::sessions::FileExists(lhs) && ios::sessions::FileExists(rhs)) {
    NSData* lhs_data = ios::sessions::ReadFile(lhs);
    NSData* rhs_data = ios::sessions::ReadFile(rhs);
    return [lhs_data isEqualToData:rhs_data];
  }

  // If either path is not a directory, then they content is not identical.
  if (!ios::sessions::DirectoryExists(lhs) ||
      !ios::sessions::DirectoryExists(rhs)) {
    return false;
  }

  // Both paths are directory, check the content recursively.
  const std::set<base::FilePath> lhs_names = DirectoryContent(lhs);
  const std::set<base::FilePath> rhs_names = DirectoryContent(rhs);
  if (lhs_names != rhs_names) {
    return false;
  }

  // If the list of items are identical, compare them recursively.
  for (const base::FilePath& name : lhs_names) {
    if (!PathAreIdentical(lhs.Append(name), rhs.Append(name))) {
      return false;
    }
  }
  return true;
}

}  // namespace

// Fake object that cannot be serialized. Used to test `ArchiveRootObject`
// failure code paths.
@interface UnserializableObject : NSObject <NSCoding>
@end

@implementation UnserializableObject

- (void)encodeWithCoder:(NSCoder*)coder {
  // Use a decoding method during encoding to cause the encoding to fail.
  [coder decodeObjectForKey:@"error"];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return nil;
}

@end

// Fake object that cannot be decoded. Used to test `DecodeRootObject`
// failure code paths.
@interface UndecodableObject : NSObject <NSCoding>
@end

@implementation UndecodableObject

- (void)encodeWithCoder:(NSCoder*)coder {
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  // Use an encoding method during decoding to cause the decoding to fail.
  if ((self = [super init])) {
    [coder encodeObject:@{} forKey:@"error"];
  }
  return self;
}

@end

// Tests that `FileExists` return true if the path corresponds to an existing
// file or false otherwise (e.g. corresponds to a directory, or does not exist).
TEST_F(SessionInternalUtilTest, FileExists) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that FileExists() returns false if the file does not exist.
  EXPECT_FALSE(ios::sessions::FileExists(root.Append(kFilename)));

  // Create a file and check that FileExists() returns true.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(root.Append(kFilename), data));
  EXPECT_TRUE(ios::sessions::FileExists(root.Append(kFilename)));

  // Create a directory and check that FileExists() returns false.
  EXPECT_TRUE(ios::sessions::CreateDirectory(root.Append(kDirname1)));
  EXPECT_FALSE(ios::sessions::FileExists(root.Append(kDirname1)));
}

// Tests that `DirectoryExists` return true if the path corresponds to an
// existing directory or false otherwise (e.g. corresponds to a file, or does
// not exist).
TEST_F(SessionInternalUtilTest, DirectoryExists) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Check that DirectoryExists() returns false if the file does not exist.
  EXPECT_FALSE(ios::sessions::DirectoryExists(root.Append(kDirname1)));

  // Create a directory and check that DirectoryExists() returns true.
  EXPECT_TRUE(ios::sessions::CreateDirectory(root.Append(kDirname1)));
  EXPECT_TRUE(ios::sessions::DirectoryExists(root.Append(kDirname1)));

  // Create a file and check that DirectoryExists() returns true.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(root.Append(kFilename), data));
  EXPECT_FALSE(ios::sessions::DirectoryExists(root.Append(kFilename)));
}

// Tests that `RenameFile` correctly move a file.
TEST_F(SessionInternalUtilTest, RenameFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFilename);
  const base::FilePath dest = root.Append(kDirname2).Append(kFilename);

  // Create a file in a sub-directory.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(from, data));

  // Check that moving the file is a success, and that the file content
  // is the expected one.
  EXPECT_TRUE(ios::sessions::RenameFile(from, dest));
  NSData* read = ios::sessions::ReadFile(dest);
  EXPECT_NSEQ(read, data);
}

// Tests that `RenameFile` fails if it cannot create destination directory.
TEST_F(SessionInternalUtilTest, RenameFile_FailureCreatingDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFilename);
  const base::FilePath dest = root.Append(kDirname2).Append(kFilename);

  // Create a file in a sub-directory.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(from, data));

  // Create a file at the path `dest.DirName()` which will cause the
  // call to `CreateDirectory` to fail.
  EXPECT_TRUE(ios::sessions::WriteFile(dest.DirName(), data));

  // Check that trying to move fail while trying to create the directory.
  EXPECT_FALSE(ios::sessions::RenameFile(from, dest));
}

// Tests that `RenameFile` fails if it cannot write the destination file.
TEST_F(SessionInternalUtilTest, RenameFile_FailureRenamingFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFilename);
  const base::FilePath dest = root.Append(kDirname2).Append(kFilename);

  // Check that trying to move a non-existent file fails.
  EXPECT_FALSE(ios::sessions::RenameFile(from, dest));
}

// Tests that `CreateDirectory` correctly create the directory, or return
// a success if the destination exists and is a directory.
TEST_F(SessionInternalUtilTest, CreateDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1).Append(kDirname2);
  EXPECT_FALSE(ios::sessions::DirectoryExists(dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(dir.DirName()));

  // Check that creating the directory succeed and that the directory
  // exists after the successful creation. If should have created the
  // parent directory too.
  EXPECT_TRUE(ios::sessions::CreateDirectory(dir));
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir));
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir.DirName()));

  // Check that trying to create an existing directory result in a success.
  EXPECT_TRUE(ios::sessions::CreateDirectory(dir));
}

// Tests that `CreateDirectory` returns false in case of failure.
TEST_F(SessionInternalUtilTest, CreateDirectory_Failure) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath file = root.Append(kFilename);

  // Create a file at the path where we will try to create the directory.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));
  EXPECT_TRUE(ios::sessions::FileExists(file));

  // Check that trying to create a directory at a path where a file exists
  // results in a failure to create the directory.
  const base::FilePath dir = file.Append(kDirname2);
  EXPECT_FALSE(ios::sessions::CreateDirectory(dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(dir));
}

// Tests that `DirectoryEmpty` returns true if the directory exists and is
// empty, false otherwise (not a directory or a directory that is not empty).
TEST_F(SessionInternalUtilTest, DirectoryEmpty) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that `DirectoryEmpty` returns false if the path is not a directory
  // (e.g. the path does not exists).
  EXPECT_FALSE(ios::sessions::DirectoryEmpty(dir));

  // Check that `DirectoryEmpty` returns true if the path is a directory and
  // the directory is empty (i.e. just created with no file inside).
  EXPECT_TRUE(ios::sessions::CreateDirectory(dir));
  EXPECT_TRUE(ios::sessions::DirectoryEmpty(dir));

  // Create a file inside `dir` and check that `DirectoryEmpty` now returns
  // false.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));
  EXPECT_TRUE(ios::sessions::FileExists(file));
  EXPECT_FALSE(ios::sessions::DirectoryEmpty(dir));
}

// Tests that `DeleteRecursively` correctly remove a file/directory and in
// the case of a directory, all its content recursively.
TEST_F(SessionInternalUtilTest, DeleteRecursively) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kDirname2).Append(kFilename);

  // Create a file deeply nested.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));
  EXPECT_TRUE(ios::sessions::FileExists(file));

  // Check that deleting `dir` delete everything below.
  EXPECT_TRUE(ios::sessions::DeleteRecursively(dir));
  EXPECT_FALSE(ios::sessions::DirectoryExists(dir));
  EXPECT_FALSE(ios::sessions::FileExists(file));

  // Create a file deeply nested.
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));
  EXPECT_TRUE(ios::sessions::FileExists(file));

  // Check that deleting `file` only delete the file and nothing else.
  EXPECT_TRUE(ios::sessions::DeleteRecursively(file));
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir.DirName()));
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir));
  EXPECT_FALSE(ios::sessions::FileExists(file));
}

// Tests that `DeleteRecursively` returns false in case of failure.
TEST_F(SessionInternalUtilTest, DeleteRecursively_Failure) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kDirname2).Append(kFilename);

  // Check that trying to delete a non-existent file fails.
  EXPECT_FALSE(ios::sessions::FileExists(file));
  EXPECT_FALSE(ios::sessions::DeleteRecursively(file));
}

// Tests that `CopyDirectory` correctly copy the directory structure
// recursively.
TEST_F(SessionInternalUtilTest, CopyDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create the source directory with some sub-directories and files.
  const base::FilePath from = root.Append(kFromName);
  const base::FilePath from_dir1 = from.Append(kDirname1);
  const base::FilePath from_dir2 = from.Append(kDirname2);

  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir1.Append(kFilename), data));
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir2.Append(kFilename), data));

  // Check that copying recursively to inexistent destination works.
  const base::FilePath dest = root.Append(kDestName);
  EXPECT_TRUE(ios::sessions::CopyDirectory(from, dest));
  EXPECT_TRUE(PathAreIdentical(from, dest));
}

// Tests that `CopyDirectory` correctly replaces the content of the target
// directory if it exists.
TEST_F(SessionInternalUtilTest, CopyDirectory_OverExistingDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create the source directory with some sub-directories and files.
  const base::FilePath from = root.Append(kFromName);
  const base::FilePath from_dir1 = from.Append(kDirname1);
  const base::FilePath from_dir2 = from.Append(kDirname2);

  NSData* data0 = [@"data0" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir1.Append(kFilename), data0));
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir2.Append(kFilename), data0));

  // Create the target directory with a different content.
  const base::FilePath dest = root.Append(kDestName);
  const base::FilePath dest_dir3 = from.Append(kDirname3);

  NSData* data1 = [@"data1" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(dest_dir3.Append(kFilename), data1));
  ASSERT_TRUE(ios::sessions::WriteFile(dest.Append(kFilename), data1));

  // Check that both directories have distinct content.
  ASSERT_FALSE(PathAreIdentical(from, dest));

  // Check that copying recursively to existing directory works and erase
  // all content in des.
  EXPECT_TRUE(ios::sessions::CopyDirectory(from, dest));
  EXPECT_TRUE(PathAreIdentical(from, dest));
}

// Tests that `CopyDirectory` succeeds even if the destination requires
// creating the parent directories.
TEST_F(SessionInternalUtilTest, CopyDirectory_TargetNestedInNonExistentDir) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create the source directory with some sub-directories and files.
  const base::FilePath from = root.Append(kFromName);
  const base::FilePath from_dir1 = from.Append(kDirname1);
  const base::FilePath from_dir2 = from.Append(kDirname2);

  NSData* data0 = [@"data0" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir1.Append(kFilename), data0));
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir2.Append(kFilename), data0));

  // Use a destination directory that is deeply nested and change that the
  // copy succeed (and has the same content as the source).
  const base::FilePath deep = root.Append(kDirname1).Append(kDirname2);
  ASSERT_FALSE(ios::sessions::DirectoryExists(deep));
  ASSERT_FALSE(ios::sessions::DirectoryExists(deep.DirName()));

  const base::FilePath dest = deep.Append(kDestName);
  EXPECT_TRUE(ios::sessions::CopyDirectory(from, dest));
  EXPECT_TRUE(PathAreIdentical(from, dest));
}

// Tests that `CopyDirectory` fails if target is a file.
TEST_F(SessionInternalUtilTest, CopyDirectory_FailureDestinationIsAFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create the source directory with some sub-directories and files.
  const base::FilePath from = root.Append(kFromName);
  const base::FilePath from_dir1 = from.Append(kDirname1);
  const base::FilePath from_dir2 = from.Append(kDirname2);

  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir1.Append(kFilename), data));
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir2.Append(kFilename), data));

  // Create a file with the same name as target directory.
  const base::FilePath dest = root.Append(kDestName);
  ASSERT_TRUE(ios::sessions::WriteFile(dest, data));

  // Check that trying to copy source over a file fails.
  EXPECT_FALSE(ios::sessions::CopyDirectory(from, dest));
}

// Tests that `CopyDirectory` fails if source is a file.
TEST_F(SessionInternalUtilTest, CopyDirectory_FailureSourceNotADirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create a file named like source.
  const base::FilePath from = root.Append(kFromName);
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from, data));

  // Check that CopyDirectory fails when the source is a file.
  const base::FilePath dest = root.Append(kDestName);
  EXPECT_FALSE(ios::sessions::CopyDirectory(from, dest));
}

// Tests that `CopyDirectory` fails if it cannot create the parent of the
// target directory.
TEST_F(SessionInternalUtilTest, CopyDirectory_FailureCannotCreateTargetParent) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create the source directory with some sub-directories and files.
  const base::FilePath from = root.Append(kFromName);
  const base::FilePath from_dir1 = from.Append(kDirname1);
  const base::FilePath from_dir2 = from.Append(kDirname2);

  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir1.Append(kFilename), data));
  ASSERT_TRUE(ios::sessions::WriteFile(from_dir2.Append(kFilename), data));

  // Use a destination directory that is deeply nested.
  const base::FilePath deep = root.Append(kDirname1).Append(kDirname2);
  const base::FilePath dest = deep.Append(kDestName);
  ASSERT_FALSE(ios::sessions::DirectoryExists(deep));
  ASSERT_FALSE(ios::sessions::DirectoryExists(deep.DirName()));

  // Create a file in the location of the target parent directory. This
  // should cause the creation of the parent directory to fail and thus
  // the failure of the copy.
  ASSERT_TRUE(ios::sessions::WriteFile(deep, data));

  // Check that the copy failed and that the file that was in the way
  // has not been modified.
  EXPECT_FALSE(ios::sessions::CopyDirectory(from, dest));
  EXPECT_NSEQ(ios::sessions::ReadFile(deep), data);
}

// Tests that `CopyFile` returns success if the file can be copied.
TEST_F(SessionInternalUtilTest, CopyFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFromName);
  const base::FilePath dest = root.Append(kDirname2).Append(kDestName);

  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from, data));

  // Check that copying the file leave the source file intact, creates
  // the directory structure for destination file, and both files have
  // the same content.
  EXPECT_TRUE(ios::sessions::CopyFile(from, dest));

  EXPECT_TRUE(ios::sessions::FileExists(from));
  EXPECT_NSEQ(ios::sessions::ReadFile(from), data);

  EXPECT_TRUE(ios::sessions::FileExists(dest));
  EXPECT_NSEQ(ios::sessions::ReadFile(dest), data);
}

// Tests that `CopyFile` returns success and overwritten destination if
// it exists and is a file.
TEST_F(SessionInternalUtilTest, CopyFile_OverwriteDestination) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFromName);
  const base::FilePath dest = root.Append(kDirname2).Append(kDestName);

  NSData* data1 = [@"data1" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from, data1));

  NSData* data2 = [@"data2" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(dest, data2));
  ASSERT_NSEQ(ios::sessions::ReadFile(dest), data2);

  // Check that copying the file leave the source file intact, overwrite the
  // destination file, and both files have the same content.
  EXPECT_TRUE(ios::sessions::CopyFile(from, dest));

  EXPECT_TRUE(ios::sessions::FileExists(from));
  EXPECT_NSEQ(ios::sessions::ReadFile(from), data1);

  EXPECT_TRUE(ios::sessions::FileExists(dest));
  EXPECT_NSEQ(ios::sessions::ReadFile(dest), data1);
}

// Tests that `CopyFile` fails if the source is not a file.
TEST_F(SessionInternalUtilTest, CopyFile_FailureSourceIsADirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFromName);
  const base::FilePath dest = root.Append(kDirname2).Append(kDestName);

  ASSERT_TRUE(ios::sessions::CreateDirectory(from));

  // Check that trying to copy `from` which is a directory using CopyFile()`
  // fails with an error.
  EXPECT_FALSE(ios::sessions::CopyFile(from, dest));
}

// Tests that `CopyFile` fails if the source does not exist.
TEST_F(SessionInternalUtilTest, CopyFile_FailureSourceMissing) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFromName);
  const base::FilePath dest = root.Append(kDirname2).Append(kDestName);

  // Check that trying to copy `from` which is a directory using CopyFile()`
  // fails with an error.
  EXPECT_FALSE(ios::sessions::CopyFile(from, dest));
}

// Tests that `CopyFile` fails if the destination path is a directory.
TEST_F(SessionInternalUtilTest, CopyFile_FailureDestinationIsADirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath from = root.Append(kDirname1).Append(kFromName);
  const base::FilePath dest = root.Append(kDirname2).Append(kDestName);

  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  ASSERT_TRUE(ios::sessions::WriteFile(from, data));
  ASSERT_TRUE(ios::sessions::CreateDirectory(dest));

  // Check that trying to copy a file to a path that is a directory fails.
  EXPECT_FALSE(ios::sessions::CopyFile(from, dest));
}

// Tests that `WriteFile` returns success when the file is created and the
// data written to disk.
TEST_F(SessionInternalUtilTest, WriteFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  EXPECT_FALSE(ios::sessions::FileExists(file));

  // Check that creating a file creates its parent directory (recursively)
  // and correctly write the data to the disk.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));
  EXPECT_TRUE(ios::sessions::DirectoryExists(dir));
  EXPECT_TRUE(ios::sessions::FileExists(file));
  NSData* read = ios::sessions::ReadFile(file);
  EXPECT_NSEQ(read, data);
}

// Tests that `WriteFile` fails if it cannot create the parent directory.
TEST_F(SessionInternalUtilTest, WriteFile_FailureCreateDirectory) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a file named `dir` which should prevent creating a directory
  // with the same path in the next call to `WriteFile`.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(dir, data));

  // Check that creating a file named `file` will fail because the parent
  // directory cannot be created.
  EXPECT_FALSE(ios::sessions::WriteFile(file, data));
}

// Tests that `WriteFile` fails if it cannot write the data.
TEST_F(SessionInternalUtilTest, WriteFile_FailureWritingFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a directory named `file` which should prevent creating a file
  // with the same path in the next call to `WriteFile`.
  EXPECT_TRUE(ios::sessions::CreateDirectory(file));

  // Check that creating a file named `file` will fail because the parent
  // directory cannot be created.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_FALSE(ios::sessions::WriteFile(file, data));
}

// Tests that `ReadFile` read the data from disk or return nil on failure.
TEST_F(SessionInternalUtilTest, ReadFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that reading from an inexistent file fails and return nil.
  EXPECT_FALSE(ios::sessions::FileExists(file));
  EXPECT_NSEQ(nil, ios::sessions::ReadFile(file));

  // Create a file with some data.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file return the written data.
  NSData* read = ios::sessions::ReadFile(file);
  EXPECT_NSEQ(read, data);
}

// Tests that `WriteProto` correctly write serialized protobuf message to disk.
TEST_F(SessionInternalUtilTest, WriteProto) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  // Create a protobuf message that is not empty.
  ios::proto::WebStateListStorage proto;
  proto.set_active_index(-1);

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that writing the protobuf message succeed and that some data
  // is written to disk.
  EXPECT_FALSE(ios::sessions::FileExists(file));
  EXPECT_TRUE(ios::sessions::WriteProto(file, proto));
  EXPECT_NSNE(nil, ios::sessions::ReadFile(file));
}

// Tests that `WriteProto` fails if it cannot serialize the protobuf message.
TEST_F(SessionInternalUtilTest, WriteProto_FailureSerializeMessage) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that writing the protobuf message succeed and that some data
  // is written to disk.
  EXPECT_FALSE(ios::sessions::FileExists(file));
  EXPECT_FALSE(ios::sessions::WriteProto(file, UnserializableMessage{}));
}

// Tests that `ParseProto` succeed when reading a protobuf message written
// using `WriteProto`.
TEST_F(SessionInternalUtilTest, ParseProto) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Serialize a non-empty protobuf to `file`.
  ios::proto::WebStateListStorage proto;
  proto.add_items()->set_identifier(10);
  proto.set_active_index(-1);
  EXPECT_TRUE(ios::sessions::WriteProto(file, proto));

  // Check that reading the protobuf message succeed and that the content
  // is identical.
  ios::proto::WebStateListStorage parsed;
  EXPECT_TRUE(ios::sessions::ParseProto(file, parsed));
  EXPECT_EQ(parsed, proto);
}

// Tests that `ParseProto` fails if it cannot read the file.
TEST_F(SessionInternalUtilTest, ParseProto_FailureReadFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that reading the protobuf message fails if the file does not exist.
  ios::proto::WebStateListStorage parsed;
  EXPECT_FALSE(ios::sessions::ParseProto(file, parsed));
}

// Tests that `ParseProto` fails if it cannot parse the file as a valid
// protobuf message.
TEST_F(SessionInternalUtilTest, ParseProto_FailureParseMessage) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Write unstructured data to the file, that is not a valid serialized
  // protobuf message.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the protobuf message fails if the data cannot be
  // parsed as a valid protobuf message.
  ios::proto::WebStateListStorage parsed;
  EXPECT_FALSE(ios::sessions::ParseProto(file, parsed));
}

// Tests that `ArchiveRootObject` returns a non-null data when serialization
// of the object is a success.
TEST_F(SessionInternalUtilTest, ArchiveRootObject) {
  NSObject<NSCoding>* root = @"data";
  NSData* data = ios::sessions::ArchiveRootObject(root);
  EXPECT_NSNE(data, nil);
}

// Tests that `ArchiveRootObject` returns nil when serialization fails.
TEST_F(SessionInternalUtilTest, ArchiveRootObject_FailureUnserializable) {
  NSObject<NSCoding>* root = [[UnserializableObject alloc] init];
  NSData* data = ios::sessions::ArchiveRootObject(root);
  EXPECT_NSEQ(data, nil);
}

// Tests that `DecodeRootObject` returns an object that is equal to the
// serialized one when invoked with the output of `ArchiveRootObject`.
TEST_F(SessionInternalUtilTest, DecodeRootObject) {
  NSObject<NSCoding>* root = @"data";
  NSData* data = ios::sessions::ArchiveRootObject(root);
  EXPECT_NSNE(data, nil);

  NSObject<NSCoding>* decoded = ios::sessions::DecodeRootObject(data);
  EXPECT_NSEQ(decoded, root);
}

// Tests that `DecodeRootObject` returns nil if the data cannot be
// parsed as a valid encoding.
TEST_F(SessionInternalUtilTest, DecodeRootObject_FailureInvalidData) {
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  NSObject<NSCoding>* decoded = ios::sessions::DecodeRootObject(data);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `DecodeRootObject` returns nil if the data is a valid
// encoding, but cannot be decoded.
TEST_F(SessionInternalUtilTest, DecodeRootObject_FailureDecodeObject) {
  UndecodableObject* root = [[UndecodableObject alloc] init];
  NSData* data = ios::sessions::ArchiveRootObject(root);
  NSObject<NSCoding>* decoded = ios::sessions::DecodeRootObject(data);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `DecodeRootObject` returns nil if the data is nil.
TEST_F(SessionInternalUtilTest, DecodeRootObject_FailureNil) {
  NSObject<NSCoding>* decoded = ios::sessions::DecodeRootObject(nil);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `ReadSessionsWindowFromPath` succeed if a file containing a
// valid `CRWSessionIOS*` encoding is written at the path.
TEST_F(SessionInternalUtilTest, ReadSessionWindow) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a fake session and write it to disk.
  SessionWindowIOS* session = CreateSessionWindowIOS();
  EXPECT_TRUE(ios::sessions::WriteSessionWindow(file, session));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, session);
}

// Tests that `ReadSessionsWindowFromPath` succeed if a file containing a
// valid `SessionIOS*` encoding with a single window is written at the path.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_SessionIOS) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a fake session and write it to disk.
  SessionWindowIOS* session = CreateSessionWindowIOS();
  SessionIOS* session_ios = [[SessionIOS alloc] initWithWindows:@[ session ]];
  NSData* data = ios::sessions::ArchiveRootObject(session_ios);
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, session);
}

// Tests that `ReadSessionsWindowFromPath` fails if the session file does
// not exists.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_FailureNoSession) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Check that reading the file fails if it does not exist.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `ReadSessionsWindowFromPath` fails if the session file cannot
// be decoded.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_FailureInvalidData) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a file containing garbage.
  NSData* data = [@"data" dataUsingEncoding:NSUTF8StringEncoding];
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file fails if it does not exist.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `ReadSessionsWindowFromPath` fails if the session file contains
// a valid `SessionsIOS*` encoding with no window.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_FailureNoWindows) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a session file containing a SessionIOS object without any window.
  SessionIOS* session_ios = [[SessionIOS alloc] initWithWindows:@[]];
  NSData* data = ios::sessions::ArchiveRootObject(session_ios);
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `ReadSessionsWindowFromPath` fails if the session file contains
// a valid `SessionsIOS*` encoding with too many windows.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_FailureExtraWindows) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a session file containing a SessionIOS object with two windows.
  SessionIOS* session_ios = [[SessionIOS alloc]
      initWithWindows:@[ CreateSessionWindowIOS(), CreateSessionWindowIOS() ]];
  NSData* data = ios::sessions::ArchiveRootObject(session_ios);
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `ReadSessionsWindowFromPath` fails if the session file contains
// a valid encoding of an unexpected type.
TEST_F(SessionInternalUtilTest, ReadSessionWindow_UnexpectedObject) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a file containing an unexpected object.
  NSData* data = ios::sessions::ArchiveRootObject(@"data");
  EXPECT_TRUE(ios::sessions::WriteFile(file, data));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, nil);
}

// Tests that `WriteSessionWindow` succeed writing the session to file.
TEST_F(SessionInternalUtilTest, WriteSessionWindow) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a fake session and serialize it to disk.
  SessionWindowIOS* session = CreateSessionWindowIOS();
  EXPECT_TRUE(ios::sessions::WriteSessionWindow(file, session));
  EXPECT_TRUE(ios::sessions::FileExists(file));

  // Check that reading the file succeed and return an object that is equal.
  SessionWindowIOS* decoded = ios::sessions::ReadSessionWindow(file);
  EXPECT_NSEQ(decoded, session);
}

// Tests that `WriteSessionWindow` fails if it cannot serialize the session.
TEST_F(SessionInternalUtilTest, WriteSessionWindow_FailureSerialization) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a CRWSessionUserData user data containing an unserializable object.
  CRWSessionUserData* user_data = [[CRWSessionUserData alloc] init];
  [user_data setObject:[[UnserializableObject alloc] init] forKey:@"error"];

  // Create a session containing an unserializable object.
  SessionWindowIOS* session = CreateSessionWindowIOS();
  session.sessions[0].userData = user_data;

  // Check that writing the session fails as the session cannot be
  // serialized and that the file is not created.
  EXPECT_FALSE(ios::sessions::WriteSessionWindow(file, session));
  EXPECT_FALSE(ios::sessions::FileExists(file));
}

// Tests that `WriteSessionWindow` fails if it cannot create the session file.
TEST_F(SessionInternalUtilTest, WriteSessionWindow_FailureWriteFile) {
  base::ScopedTempDir scoped_temp_dir;
  ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir());
  const base::FilePath root = scoped_temp_dir.GetPath();

  const base::FilePath dir = root.Append(kDirname1);
  const base::FilePath file = dir.Append(kFilename);

  // Create a directory at the same location as the session file.
  EXPECT_TRUE(ios::sessions::CreateDirectory(file));

  // Check that writing the session fails as the session cannot be
  // serialized and that the file is not created.
  SessionWindowIOS* session = CreateSessionWindowIOS();
  EXPECT_FALSE(ios::sessions::WriteSessionWindow(file, session));
}
