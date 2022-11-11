// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ERROR_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ERROR_H_

#include "base/files/file.h"
#include "base/strings/string_piece.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace content {
namespace file_system_access_error {

// Returns a FileSystemAccessError representing a successful result of an
// operation.
CONTENT_EXPORT blink::mojom::FileSystemAccessErrorPtr Ok();

// Wraps a base::File::Error in a FileSystemAccessError, optionally with a
// custom error message.
blink::mojom::FileSystemAccessErrorPtr FromFileError(
    base::File::Error result,
    base::StringPiece message = "");

// Wraps a FileSystemAccessStatus in a FileSystemAccessError, optionally with a
// custom error message.
blink::mojom::FileSystemAccessErrorPtr FromStatus(
    blink::mojom::FileSystemAccessStatus status,
    base::StringPiece message = "");

}  // namespace file_system_access_error
}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_ERROR_H_
