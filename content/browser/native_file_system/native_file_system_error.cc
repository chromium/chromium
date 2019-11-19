// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_error.h"

namespace content {

using blink::mojom::NativeFileSystemError;
using blink::mojom::NativeFileSystemErrorPtr;
using blink::mojom::NativeFileSystemStatus;

namespace native_file_system_error {

NativeFileSystemErrorPtr Ok() {
  return NativeFileSystemError::New(NativeFileSystemStatus::kOk,
                                    base::File::FILE_OK, "");
}

NativeFileSystemErrorPtr FromFileError(base::File::Error result,
                                       base::StringPiece message) {
  if (result == base::File::FILE_OK)
    return Ok();
  return NativeFileSystemError::New(NativeFileSystemStatus::kFileError, result,
                                    std::string(message));
}

blink::mojom::NativeFileSystemErrorPtr FromStatus(
    blink::mojom::NativeFileSystemStatus status,
    base::StringPiece message) {
  return NativeFileSystemError::New(status, base::File::FILE_OK,
                                    std::string(message));
}

}  // namespace native_file_system_error
}  // namespace content
