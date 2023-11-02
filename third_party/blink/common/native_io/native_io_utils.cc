// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "third_party/blink/public/common/native_io/native_io_utils.h"

#include "base/files/file.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-shared.h"

namespace blink {
namespace native_io {

// See https://crbug.com/1095537 for a design doc of this mapping.
blink::mojom::NativeIOErrorType FileErrorToNativeIOErrorType(
    const base::File::Error error) {
  switch (error) {
    case base::File::FILE_OK:
      return mojom::NativeIOErrorType::kSuccess;
    // Errors in this category are unexpected and provide no way of recovery.
    case base::File::FILE_ERROR_ABORT:
    case base::File::FILE_ERROR_INVALID_OPERATION:
    case base::File::FILE_ERROR_INVALID_URL:
    case base::File::FILE_ERROR_IO:
    case base::File::FILE_ERROR_NOT_A_DIRECTORY:
    case base::File::FILE_ERROR_NOT_A_FILE:
    case base::File::FILE_ERROR_NOT_EMPTY:
      return mojom::NativeIOErrorType::kUnknown;
    // Errors in this category have no recovery path within NativeIO. NOT_FOUND
    // is included here, as Windows returns NOT_FOUND when attempting to use an
    // overly long file name.
    case base::File::FILE_ERROR_ACCESS_DENIED:
    case base::File::FILE_ERROR_SECURITY:
    case base::File::FILE_ERROR_FAILED:
      return mojom::NativeIOErrorType::kInvalidState;
    case base::File::FILE_ERROR_NOT_FOUND:
      return mojom::NativeIOErrorType::kNotFound;
    // Errors in this category have a recovery path.
    case base::File::FILE_ERROR_EXISTS:
    case base::File::FILE_ERROR_IN_USE:
    case base::File::FILE_ERROR_NO_MEMORY:
    case base::File::FILE_ERROR_TOO_MANY_OPENED:
      return mojom::NativeIOErrorType::kNoModificationAllowed;
    case base::File::FILE_ERROR_NO_SPACE:
      return mojom::NativeIOErrorType::kNoSpace;
    case base::File::FILE_ERROR_MAX:
      NOTREACHED();
      return mojom::NativeIOErrorType::kUnknown;
  }
  NOTREACHED();
  return mojom::NativeIOErrorType::kUnknown;
}

std::string GetDefaultMessage(const mojom::NativeIOErrorType nativeio_error) {
  switch (nativeio_error) {
    case mojom::NativeIOErrorType::kSuccess:
      return "";
    case mojom::NativeIOErrorType::kUnknown:
      return "Unspecified internal error.";
    case mojom::NativeIOErrorType::kInvalidState:
      return "Operation failed.";
    case mojom::NativeIOErrorType::kNotFound:
      return "File not found.";
    case mojom::NativeIOErrorType::kNoModificationAllowed:
      return "No modification allowed.";
    case mojom::NativeIOErrorType::kNoSpace:
      return "No space available.";
  }
  NOTREACHED();
  return std::string();
}

}  // namespace native_io
}  // namespace blink
