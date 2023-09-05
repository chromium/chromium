// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_internal_util.h"

#import "base/apple/foundation_util.h"
#import "base/files/file_path.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "third_party/protobuf/src/google/protobuf/message_lite.h"

namespace ios::sessions {
namespace {

// Option for writing data.
constexpr NSDataWritingOptions kWritingOptions =
    NSDataWritingAtomic |
    NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;

// Indicate the status for a path.
enum class PathStatus {
  kFile,
  kDirectory,
  kInexistent,
};

// Checks the status of `path`.
[[nodiscard]] PathStatus GetPathStatus(const base::FilePath& path) {
  BOOL is_directory = NO;
  NSString* name = base::apple::FilePathToNSString(path);
  if (![[NSFileManager defaultManager] fileExistsAtPath:name
                                            isDirectory:&is_directory]) {
    return PathStatus::kInexistent;
  }
  return is_directory ? PathStatus::kDirectory : PathStatus::kFile;
}

}  // namespace

bool FileExists(const base::FilePath& filename) {
  return GetPathStatus(filename) == PathStatus::kFile;
}

bool DirectoryExists(const base::FilePath& dirname) {
  return GetPathStatus(dirname) == PathStatus::kDirectory;
}

bool RenameFile(const base::FilePath& from, const base::FilePath& dest) {
  if (!CreateDirectory(dest.DirName())) {
    return false;
  }

  NSError* error = nil;
  NSString* from_path = base::apple::FilePathToNSString(from);
  NSString* dest_path = base::apple::FilePathToNSString(dest);
  if (![[NSFileManager defaultManager] moveItemAtPath:from_path
                                               toPath:dest_path
                                                error:&error]) {
    DLOG(WARNING) << "Error moving file from: "
                  << base::SysNSStringToUTF8(from_path)
                  << " to: " << base::SysNSStringToUTF8(dest_path) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

bool CreateDirectory(const base::FilePath& directory) {
  NSError* error = nil;
  NSString* path = base::apple::FilePathToNSString(directory);
  if (![[NSFileManager defaultManager] createDirectoryAtPath:path
                                 withIntermediateDirectories:YES
                                                  attributes:nil
                                                       error:&error]) {
    DLOG(WARNING) << "Error creating directory: "
                  << base::SysNSStringToUTF8(path) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

bool DirectoryEmpty(const base::FilePath& directory) {
  if (!DirectoryExists(directory)) {
    return false;
  }

  NSString* path = base::apple::FilePathToNSString(directory);
  NSDirectoryEnumerator* enumerator =
      [[NSFileManager defaultManager] enumeratorAtPath:path];

  return [enumerator nextObject] == nil;
}

bool DeleteRecursively(const base::FilePath& path) {
  NSError* error = nil;
  NSString* name = base::apple::FilePathToNSString(path);
  if (![[NSFileManager defaultManager] removeItemAtPath:name error:&error]) {
    DLOG(WARNING) << "Error removing file/directory at: "
                  << base::SysNSStringToUTF8(name) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

bool WriteFile(const base::FilePath& filename, NSData* data) {
  if (!CreateDirectory(filename.DirName())) {
    return false;
  }

  NSError* error = nil;
  NSString* path = base::apple::FilePathToNSString(filename);
  if (![data writeToFile:path options:kWritingOptions error:&error]) {
    DLOG(WARNING) << "Error writing to file: " << base::SysNSStringToUTF8(path)
                  << ": " << base::SysNSStringToUTF8([error description]);
    return false;
  }

  return true;
}

NSData* ReadFile(const base::FilePath& filename) {
  NSError* error = nil;
  NSString* path = base::apple::FilePathToNSString(filename);
  NSData* data = [NSData dataWithContentsOfFile:path options:0 error:&error];
  if (!data) {
    DLOG(WARNING) << "Error loading from file: "
                  << base::SysNSStringToUTF8(path) << ": "
                  << base::SysNSStringToUTF8([error description]);
    return nil;
  }

  return data;
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
