// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/async_file_util.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

namespace remoting {

namespace {

base::TaskTraits GetFileTaskTraits() {
  return {base::MayBlock(), base::TaskPriority::BEST_EFFORT};
}

// Wrapper around base::ReadFileToString that returns the result as an optional
// string.
std::optional<std::string> ReadFileToString(const base::FilePath& file) {
  std::string result;
  bool success = base::ReadFileToString(file, &result);
  if (success) {
    return result;
  } else {
    return std::nullopt;
  }
}

}  // namespace

void WriteFileAsync(const base::FilePath& file,
                    std::string_view content,
                    base::OnceCallback<void(bool)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(),
      base::BindOnce(
          [](base::FilePath file, std::string content) {
            return base::WriteFile(file, content);
          },
          file, std::string(content)),
      std::move(on_done));
}

void ReadFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(std::optional<std::string>)> on_done) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, GetFileTaskTraits(), base::BindOnce(&ReadFileToString, file),
      std::move(on_done));
}

}  // namespace remoting
