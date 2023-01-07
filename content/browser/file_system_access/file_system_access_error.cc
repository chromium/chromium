// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_error.h"

namespace content {

using blink::mojom::FileSystemAccessError;
using blink::mojom::FileSystemAccessErrorPtr;
using blink::mojom::FileSystemAccessStatus;

namespace file_system_access_error {

FileSystemAccessErrorPtr Ok() {
  return FileSystemAccessError::New(FileSystemAccessStatus::kOk,
                                    base::File::FILE_OK, "");
}

FileSystemAccessErrorPtr FromFileError(base::File::Error result,
                                       base::StringPiece message) {
  if (result == base::File::FILE_OK)
    return Ok();
  return FileSystemAccessError::New(FileSystemAccessStatus::kFileError, result,
                                    std::string(message));
}

blink::mojom::FileSystemAccessErrorPtr FromStatus(
    blink::mojom::FileSystemAccessStatus status,
    base::StringPiece message) {
  return FileSystemAccessError::New(status, base::File::FILE_OK,
                                    std::string(message));
}

}  // namespace file_system_access_error
}  // namespace content
