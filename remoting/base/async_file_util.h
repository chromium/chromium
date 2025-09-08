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
#include "base/types/expected.h"

namespace remoting {

void WriteFileAsync(const base::FilePath& file,
                    std::string_view content,
                    base::OnceCallback<void(base::FileErrorOr<void>)> on_done);

void ReadFileAsync(
    const base::FilePath& file,
    base::OnceCallback<void(base::FileErrorOr<std::string>)> on_done);

void DeleteFileAsync(const base::FilePath& file,
                     base::OnceCallback<void(base::FileErrorOr<void>)> on_done);

void FileExistsAsync(const base::FilePath& file,
                     base::OnceCallback<void(bool)> on_done);

}  // namespace remoting

#endif  // REMOTING_BASE_ASYNC_FILE_UTIL_H_
