// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_ASYNC_FILE_UTIL_H_
#define REMOTING_BASE_ASYNC_FILE_UTIL_H_

#include <string>
#include <string_view>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

#if BUILDFLAG(IS_POSIX)
#include <sys/types.h>
#endif

namespace remoting {

void WriteFileAsync(const base::FilePath& file,
                    std::string_view content,
                    base::OnceCallback<void(base::FileErrorOr<void>)> on_done);

// Writes the file atomically using base::ImportantFileWriter, ensuring the
// parent directory exists first (with secure default 0700 permissions).
// Runs on the provided sequenced task runner to prevent race conditions.
void WriteImportantFileAndEnsureParentDirAsync(
    const base::FilePath& file,
    std::string content,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    base::OnceCallback<void(base::FileErrorOr<void>)> on_done);

#if BUILDFLAG(IS_POSIX)
void WriteFileWithPermissionsAsync(
    const base::FilePath& file,
    std::string_view content,
    mode_t permissions,
    base::OnceCallback<void(base::FileErrorOr<void>)> on_done);
#endif

void ReadFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(base::FileErrorOr<std::string>)> on_done);

void DeleteFileAsync(const base::FilePath& file,
                     base::OnceCallback<void(base::FileErrorOr<void>)> on_done);

void FileExistsAsync(const base::FilePath& file,
                     base::OnceCallback<void(bool)> on_done);

}  // namespace remoting

#endif  // REMOTING_BASE_ASYNC_FILE_UTIL_H_
