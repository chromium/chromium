// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/choose_file/choose_file_file_utils.h"

#import <Foundation/Foundation.h>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"

namespace {

// Cleanups `root` except maybe for `session_directory`. May be blocking.
void DeleteTempChooseFileDirectorySync(base::FilePath root,
                                       std::string session_directory) {
  if (!DirectoryExists(root)) {
    return;
  }
  base::FileEnumerator enumerator(root, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    if (path.BaseName().value() == session_directory) {
      continue;
    }
    DeletePathRecursively(path);
  }
}

// Delete `root`. May be blocking.
void DeleteTempChooseFileDirectoryForTabSync(base::FilePath root) {
  DeletePathRecursively(root);
}

// Returns a UUID for the current session.
std::string GetChooseFileSession() {
  static std::string session_id(
      base::SysNSStringToUTF8([NSUUID UUID].UUIDString));
  return session_id;
}

// Return the global firectory to temporarily store file to be uploaded.
std::optional<base::FilePath> GetTempChooseFileDirectory() {
  base::FilePath directory_path;
  if (!GetTempDir(&directory_path)) {
    return std::nullopt;
  }
  directory_path = directory_path.Append("choose_file");
  return directory_path;
}

// Return the session firectory to temporarily store file to be uploaded.
std::optional<base::FilePath> GetSessionChooseFileDirectory() {
  std::optional<base::FilePath> choose_file_directory =
      GetTempChooseFileDirectory();
  if (!choose_file_directory) {
    return std::nullopt;
  }
  return (*choose_file_directory).Append(GetChooseFileSession());
}

}  // namespace

std::optional<base::FilePath> GetTabChooseFileDirectory(
    web::WebStateID web_state_id) {
  std::optional<base::FilePath> session_directory =
      GetSessionChooseFileDirectory();
  if (!session_directory) {
    return std::nullopt;
  }
  return (*session_directory)
      .Append(base::NumberToString(web_state_id.identifier()));
}

void DeleteTempChooseFileDirectoryForTab(web::WebStateID web_state_id) {
  std::optional<base::FilePath> web_state_directory =
      GetTabChooseFileDirectory(web_state_id);
  if (!web_state_directory) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTempChooseFileDirectoryForTabSync,
                     *web_state_directory));
}

void DeleteTempChooseFileDirectory() {
  std::optional<base::FilePath> choose_file_directory =
      GetTempChooseFileDirectory();
  if (!choose_file_directory) {
    return;
  }
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTempChooseFileDirectorySync, *choose_file_directory,
                     GetChooseFileSession()));
}
