// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_internal_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/model/session_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ios::sessions {
namespace internal {

// Indicate the status for a path.
enum class PathStatus {
  kFile,
  kDirectory,
  kInexistent,
};

// Checks the status of `path`.
[[nodiscard]] PathStatus GetPathStatus(NSString* path) {
  BOOL is_directory = NO;
  if (![[NSFileManager defaultManager] fileExistsAtPath:path
                                            isDirectory:&is_directory]) {
    return PathStatus::kInexistent;
  }
  return is_directory ? PathStatus::kDirectory : PathStatus::kFile;
}

// Returns whether a file named `filename` exists.
[[nodiscard]] bool FileExists(NSString* filename) {
  return GetPathStatus(filename) == PathStatus::kFile;
}

// Returns whether a directory named `dirname` exists.
[[nodiscard]] bool DirectoryExists(NSString* dirname) {
  return GetPathStatus(dirname) == PathStatus::kDirectory;
}

// Creates `directory` including all intermediate directories and returns
// whether the operation was a success. Safe to call if `directory` exists.
[[nodiscard]] bool CreateDirectory(NSString* directory) {
  NSError* error = nil;
  if (![[NSFileManager defaultManager] createDirectoryAtPath:directory
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:&error]) {
    DLOG(WARNING) << "Error creating directory: "
                  << base::SysNSStringToUTF8(directory) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Renames a file from `from` to `dest`.
[[nodiscard]] bool RenameFile(NSString* from, NSString* dest) {
  if (!CreateDirectory([dest stringByDeletingLastPathComponent])) {
    return false;
  }

  NSError* error = nil;
  if (![[NSFileManager defaultManager] moveItemAtPath:from
                                               toPath:dest
                                                error:&error]) {
    DLOG(WARNING) << "Error moving file from: " << base::SysNSStringToUTF8(from)
                  << " to: " << base::SysNSStringToUTF8(dest) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Returns whether `directory` exists and is empty.
[[nodiscard]] bool DirectoryEmpty(NSString* directory) {
  if (!DirectoryExists(directory)) {
    return false;
  }

  NSDirectoryEnumerator<NSString*>* enumerator =
      [[NSFileManager defaultManager] enumeratorAtPath:directory];

  return [enumerator nextObject] == nil;
}

// Deletes recursively file or directory at `path`. Returns whether the
// operation was a success.
[[nodiscard]] bool DeleteRecursively(NSString* path) {
  NSError* error = nil;
  if (![[NSFileManager defaultManager] removeItemAtPath:path error:&error]) {
    DLOG(WARNING) << "Error removing file/directory at: "
                  << base::SysNSStringToUTF8(path) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Copies content of `from_dir` to `dest_dir` recursively. It is an error
// if `from_dir` is not an existing directory or if `dest_dir` exists and
// is not a directory.
[[nodiscard]] bool CopyDirectory(NSString* from_dir, NSString* dest_dir) {
  if (!DirectoryExists(from_dir)) {
    DLOG(WARNING) << "Error copying directory: "
                  << base::SysNSStringToUTF8(from_dir) << " to "
                  << base::SysNSStringToUTF8(dest_dir) << ": no such directory";
    return false;
  }

  switch (GetPathStatus(dest_dir)) {
    case PathStatus::kDirectory:
      if (!DeleteRecursively(dest_dir)) {
        return false;
      }
      break;

    case PathStatus::kFile:
      DLOG(WARNING) << "Error copying directory: "
                    << base::SysNSStringToUTF8(from_dir) << " to "
                    << base::SysNSStringToUTF8(dest_dir) << ": file exists";
      return false;

    case PathStatus::kInexistent:
      break;
  }

  // Create parent directory of `dest_dir`.
  if (!CreateDirectory([dest_dir stringByDeletingLastPathComponent])) {
    return false;
  }

  // Use hardlink to perform the copy to reduce the impact on storage. The
  // documentation of -linkItemAtPath:toPath:error: explicitly explain that
  // if source is a directory, the method create the destination directory
  // and hard-link the content recursively.

  NSError* error = nil;
  if (![[NSFileManager defaultManager] linkItemAtPath:from_dir
                                               toPath:dest_dir
                                                error:&error]) {
    DLOG(WARNING) << "Error copying directory: "
                  << base::SysNSStringToUTF8(from_dir) << " to "
                  << base::SysNSStringToUTF8(dest_dir) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Copies file at `from_path` to `dest_path`. It is an error if `from_path`
// is not a file or if `dest_path` exists and is not a file.
[[nodiscard]] bool CopyFile(NSString* from_path, NSString* dest_path) {
  if (!FileExists(from_path)) {
    DLOG(WARNING) << "Error copying file: "
                  << base::SysNSStringToUTF8(from_path) << " to "
                  << base::SysNSStringToUTF8(dest_path) << ": no such file";
    return false;
  }

  switch (GetPathStatus(dest_path)) {
    case PathStatus::kDirectory:
      DLOG(WARNING) << "Error copying file: "
                    << base::SysNSStringToUTF8(from_path) << " to "
                    << base::SysNSStringToUTF8(dest_path)
                    << ": directory exists";
      break;

    case PathStatus::kFile:
      if (!DeleteRecursively(dest_path)) {
        return false;
      }
      break;

    case PathStatus::kInexistent:
      break;
  }

  // Create parent directory of `dest_path`.
  if (!CreateDirectory([dest_path stringByDeletingLastPathComponent])) {
    return false;
  }

  // Use hardlink to perform the copy to reduce the impact on storage.

  NSError* error = nil;
  if (![[NSFileManager defaultManager] linkItemAtPath:from_path
                                               toPath:dest_path
                                                error:&error]) {
    DLOG(WARNING) << "Error copying file: "
                  << base::SysNSStringToUTF8(from_path) << " to "
                  << base::SysNSStringToUTF8(dest_path) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Writes `data` to `filename` and returns whether the operation was a success.
// The file is created with protection until first user authentication.
[[nodiscard]] bool WriteFile(NSString* filename, NSData* data) {
  if (!CreateDirectory([filename stringByDeletingLastPathComponent])) {
    return false;
  }

  // Options for writing data.
  constexpr NSDataWritingOptions options =
      NSDataWritingAtomic |
      NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;

  NSError* error = nil;
  if (![data writeToFile:filename options:options error:&error]) {
    DLOG(WARNING) << "Error writing to file: "
                  << base::SysNSStringToUTF8(filename) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

// Reads content of `filename` and returns it as a `NSData*` or nil on error.
[[nodiscard]] NSData* ReadFile(NSString* filename) {
  NSError* error = nil;
  NSData* data = [NSData dataWithContentsOfFile:filename
                                        options:0
                                          error:&error];
  if (!data) {
    DLOG(WARNING) << "Error loading from file: "
                  << base::SysNSStringToUTF8(filename) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  return data;
}

}  // namespace internal

bool FileExists(const base::FilePath& filename) {
  return internal::FileExists(base::apple::FilePathToNSString(filename));
}

bool DirectoryExists(const base::FilePath& dirname) {
  return internal::DirectoryExists(base::apple::FilePathToNSString(dirname));
}

bool RenameFile(const base::FilePath& from, const base::FilePath& dest) {
  return internal::RenameFile(base::apple::FilePathToNSString(from),
                              base::apple::FilePathToNSString(dest));
}

bool CreateDirectory(const base::FilePath& directory) {
  return internal::CreateDirectory(base::apple::FilePathToNSString(directory));
}

bool DirectoryEmpty(const base::FilePath& directory) {
  return internal::DirectoryEmpty(base::apple::FilePathToNSString(directory));
}

bool DeleteRecursively(const base::FilePath& path) {
  return internal::DeleteRecursively(base::apple::FilePathToNSString(path));
}

bool CopyDirectory(const base::FilePath& from_dir,
                   const base::FilePath& dest_dir) {
  return internal::CopyDirectory(base::apple::FilePathToNSString(from_dir),
                                 base::apple::FilePathToNSString(dest_dir));
}

bool CopyFile(const base::FilePath& from_path,
              const base::FilePath& dest_path) {
  return internal::CopyFile(base::apple::FilePathToNSString(from_path),
                            base::apple::FilePathToNSString(dest_path));
}

bool WriteFile(const base::FilePath& filename, NSData* data) {
  return internal::WriteFile(base::apple::FilePathToNSString(filename), data);
}

NSData* ReadFile(const base::FilePath& filename) {
  return internal::ReadFile(base::apple::FilePathToNSString(filename));
}

bool WriteProto(const base::FilePath& filename,
                const google::protobuf::MessageLite& proto) {
  @autoreleasepool {
    // Allocate a NSData object large enough to hold the serialized protobuf.
    const size_t serialized_size = proto.ByteSizeLong();
    NSMutableData* data = [NSMutableData dataWithLength:serialized_size];

    if (!proto.SerializeToArray(data.mutableBytes, data.length)) {
      DLOG(WARNING) << "Error serializing proto to file: "
                    << filename.AsUTF8Unsafe();
      return false;
    }

    return WriteFile(filename, data);
  }
}

bool ParseProto(const base::FilePath& filename,
                google::protobuf::MessageLite& proto) {
  NSData* data = ReadFile(filename);
  if (!data) {
    return false;
  }

  if (!proto.ParseFromArray(data.bytes, data.length)) {
    DLOG(WARNING) << "Error parsing proto from file: "
                  << filename.AsUTF8Unsafe();
    return false;
  }

  return true;
}

NSData* ArchiveRootObject(NSObject<NSCoding>* object) {
  NSError* error = nil;
  NSData* archived = [NSKeyedArchiver archivedDataWithRootObject:object
                                           requiringSecureCoding:NO
                                                           error:&error];

  if (error) {
    DLOG(WARNING) << "Error serializing data: "
                  << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  return archived;
}

NSObject<NSCoding>* DecodeRootObject(NSData* data) {
  NSError* error = nil;
  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
  if (error) {
    DLOG(WARNING) << "Error deserializing data: "
                  << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  unarchiver.requiresSecureCoding = NO;

  // -decodeObjectForKey: propagates exception, so wrap the call in
  // @try/@catch block to prevent them from terminating the app.
  @try {
    return [unarchiver decodeObjectForKey:@"root"];
  } @catch (NSException* exception) {
    DLOG(WARNING) << "Error deserializing data: "
                  << base::SysNSStringToUTF8([exception description]);
    return nil;
  }
}

SessionWindowIOS* ReadSessionWindow(const base::FilePath& filename) {
  NSData* data = ReadFile(filename);
  if (!data) {
    return nil;
  }

  NSObject* root = DecodeRootObject(data);
  if (!root) {
    return nil;
  }

  if ([root isKindOfClass:[SessionIOS class]]) {
    SessionIOS* session = base::apple::ObjCCastStrict<SessionIOS>(root);
    if (session.sessionWindows.count != 1) {
      DLOG(WARNING) << "Error deserializing data: "
                    << "not exactly one SessionWindowIOS.";
      return nil;
    }

    return session.sessionWindows[0];
  }

  return base::apple::ObjCCast<SessionWindowIOS>(root);
}

bool WriteSessionWindow(const base::FilePath& filename,
                        SessionWindowIOS* session) {
  NSData* data = ArchiveRootObject(session);
  if (!data) {
    return false;
  }

  return WriteFile(filename, data);
}

}  // namespace ios::sessions
