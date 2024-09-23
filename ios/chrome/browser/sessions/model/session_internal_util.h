// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_INTERNAL_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_INTERNAL_UTIL_H_

#import <Foundation/Foundation.h>

@class SessionWindowIOS;

namespace base {
class FilePath;
}  // namespace base

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace ios::sessions {

// Returns whether a file named `filename` exists.
[[nodiscard]] bool FileExists(const base::FilePath& filename);

// Returns whether a directory named `dirname` exists.
[[nodiscard]] bool DirectoryExists(const base::FilePath& dirname);

// Renames a file from `from` to `dest`.
[[nodiscard]] bool RenameFile(const base::FilePath& from,
                              const base::FilePath& dest);

// Creates `directory` including all intermediate directories and returns
// whether the operation was a success. Safe to call if `directory` exists.
[[nodiscard]] bool CreateDirectory(const base::FilePath& directory);

// Returns whether `directory` exists and is empty.
[[nodiscard]] bool DirectoryEmpty(const base::FilePath& directory);

// Deletes recursively file or directory at `path`. Returns whether the
// operation was a success.
[[nodiscard]] bool DeleteRecursively(const base::FilePath& path);

// Copies content of `from_dir` to `dest_dir` recursively. It is an error
// if `from_dir` is not an existing directory or if `dest_dir` exists and
// is not a directory.
[[nodiscard]] bool CopyDirectory(const base::FilePath& from_dir,
                                 const base::FilePath& dest_dir);

// Copies file at `from_path` to `dest_path`. It is an error if `from_path`
// is not a file or if `dest_path` exists and is not a file.
[[nodiscard]] bool CopyFile(const base::FilePath& from_path,
                            const base::FilePath& dest_path);

// Writes `data` to `filename` and returns whether the operation was a success.
// The file is created with protection until first user authentication.
[[nodiscard]] bool WriteFile(const base::FilePath& filename, NSData* data);

// Reads content of `filename` and returns it as a `NSData*` or nil on error.
[[nodiscard]] NSData* ReadFile(const base::FilePath& filename);

// Writes `proto` to `filename` and returns whether the operation was a success.
// The file is created with protection until first user authentication.
[[nodiscard]] bool WriteProto(const base::FilePath& filename,
                              const google::protobuf::MessageLite& proto);

// Parses content of `filename` into `proto` and return whether the operation
// was a success.
[[nodiscard]] bool ParseProto(const base::FilePath& filename,
                              google::protobuf::MessageLite& proto);

// Encodes `object` into binary data. On error, returns nil.
[[nodiscard]] NSData* ArchiveRootObject(NSObject<NSCoding>* object);

// Decodes root object from `data`. On error, returns nil.
[[nodiscard]] NSObject<NSCoding>* DecodeRootObject(NSData* data);

// Loads session from `filename`. On error, returns nil.
[[nodiscard]] SessionWindowIOS* ReadSessionWindow(
    const base::FilePath& filename);

// Writes `session` to `filename`. On error, returns false.
[[nodiscard]] bool WriteSessionWindow(const base::FilePath& filename,
                                      SessionWindowIOS* session);

}  // namespace ios::sessions

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_INTERNAL_UTIL_H_
