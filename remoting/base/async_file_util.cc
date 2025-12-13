// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/async_file_util.h"

#include <optional>

#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"

namespace remoting {

namespace {

base::TaskTraits GetFileTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::BEST_EFFORT};
}

// Wrapper around base::ReadFileToString that returns the result as a
// base::FileErrorOr<std::string>.
base::FileErrorOr<std::string> ReadFileToString(const base::FilePath& file) {
  std::string result;
  bool success = base::ReadFileToString(file, &result);
  if (success) {
    return base::ok(result);
  } else {
    return base::unexpected(base::File::GetLastFileError());
  }
}

}  // namespace

void WriteFileAsync(const base::FilePath& file,
                    std::string_view content,
                    base::OnceCallback<void(base::FileErrorOr<void>)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(),
      base::BindOnce(
          [](base::FilePath file,
             std::string content) -> base::FileErrorOr<void> {
            if (base::WriteFile(file, content)) {
              return base::ok();
            } else {
              return base::unexpected(base::File::GetLastFileError());
            }
          },
          file, std::string(content)),
      std::move(on_done));
}

void ReadFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(base::FileErrorOr<std::string>)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&ReadFileToString, file),
      std::move(on_done));
}

void DeleteFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(base::FileErrorOr<void>)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(),
      base::BindOnce(
          [](base::FilePath file) -> base::FileErrorOr<void> {
            if (base::DeleteFile(file)) {
              return base::ok();
            } else {
              return base::unexpected(base::File::GetLastFileError());
            }
          },
          file),
      std::move(on_done));
}

void FileExistsAsync(const base::FilePath& file,
                     base::OnceCallback<void(bool)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&base::PathExists, file),
      std::move(on_done));
}

}  // namespace remoting
