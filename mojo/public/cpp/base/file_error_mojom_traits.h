// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_FILE_ERROR_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_FILE_ERROR_MOJOM_TRAITS_H_

#include "base/files/file.h"
#include "base/notreached.h"
#include "mojo/public/mojom/base/file_error.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<mojo_base::mojom::FileError, base::File::Error> {
  static mojo_base::mojom::FileError ToMojom(base::File::Error error) {
    switch (error) {
      case base::File::FILE_OK:
        return mojo_base::mojom::FileError::OK;
      case base::File::FILE_ERROR_FAILED:
        return mojo_base::mojom::FileError::FAILED;
      case base::File::FILE_ERROR_IN_USE:
        return mojo_base::mojom::FileError::IN_USE;
      case base::File::FILE_ERROR_EXISTS:
        return mojo_base::mojom::FileError::EXISTS;
      case base::File::FILE_ERROR_NOT_FOUND:
        return mojo_base::mojom::FileError::NOT_FOUND;
      case base::File::FILE_ERROR_ACCESS_DENIED:
        return mojo_base::mojom::FileError::ACCESS_DENIED;
      case base::File::FILE_ERROR_TOO_MANY_OPENED:
        return mojo_base::mojom::FileError::TOO_MANY_OPENED;
      case base::File::FILE_ERROR_NO_MEMORY:
        return mojo_base::mojom::FileError::NO_MEMORY;
      case base::File::FILE_ERROR_NO_SPACE:
        return mojo_base::mojom::FileError::NO_SPACE;
      case base::File::FILE_ERROR_NOT_A_DIRECTORY:
        return mojo_base::mojom::FileError::NOT_A_DIRECTORY;
      case base::File::FILE_ERROR_INVALID_OPERATION:
        return mojo_base::mojom::FileError::INVALID_OPERATION;
      case base::File::FILE_ERROR_SECURITY:
        return mojo_base::mojom::FileError::SECURITY;
      case base::File::FILE_ERROR_ABORT:
        return mojo_base::mojom::FileError::ABORT;
      case base::File::FILE_ERROR_NOT_A_FILE:
        return mojo_base::mojom::FileError::NOT_A_FILE;
      case base::File::FILE_ERROR_NOT_EMPTY:
        return mojo_base::mojom::FileError::NOT_EMPTY;
      case base::File::FILE_ERROR_INVALID_URL:
        return mojo_base::mojom::FileError::INVALID_URL;
      case base::File::FILE_ERROR_IO:
        return mojo_base::mojom::FileError::IO;
      case base::File::FILE_ERROR_MAX:
        return mojo_base::mojom::FileError::FAILED;
    }
    NOTREACHED();
  }

  static bool FromMojom(mojo_base::mojom::FileError in,
                        base::File::Error* out) {
    switch (in) {
      case mojo_base::mojom::FileError::OK:
        *out = base::File::FILE_OK;
        return true;
      case mojo_base::mojom::FileError::FAILED:
        *out = base::File::FILE_ERROR_FAILED;
        return true;
      case mojo_base::mojom::FileError::IN_USE:
        *out = base::File::FILE_ERROR_IN_USE;
        return true;
      case mojo_base::mojom::FileError::EXISTS:
        *out = base::File::FILE_ERROR_EXISTS;
        return true;
      case mojo_base::mojom::FileError::NOT_FOUND:
        *out = base::File::FILE_ERROR_NOT_FOUND;
        return true;
      case mojo_base::mojom::FileError::ACCESS_DENIED:
        *out = base::File::FILE_ERROR_ACCESS_DENIED;
        return true;
      case mojo_base::mojom::FileError::TOO_MANY_OPENED:
        *out = base::File::FILE_ERROR_TOO_MANY_OPENED;
        return true;
      case mojo_base::mojom::FileError::NO_MEMORY:
        *out = base::File::FILE_ERROR_NO_MEMORY;
        return true;
      case mojo_base::mojom::FileError::NO_SPACE:
        *out = base::File::FILE_ERROR_NO_SPACE;
        return true;
      case mojo_base::mojom::FileError::NOT_A_DIRECTORY:
        *out = base::File::FILE_ERROR_NOT_A_DIRECTORY;
        return true;
      case mojo_base::mojom::FileError::INVALID_OPERATION:
        *out = base::File::FILE_ERROR_INVALID_OPERATION;
        return true;
      case mojo_base::mojom::FileError::SECURITY:
        *out = base::File::FILE_ERROR_SECURITY;
        return true;
      case mojo_base::mojom::FileError::ABORT:
        *out = base::File::FILE_ERROR_ABORT;
        return true;
      case mojo_base::mojom::FileError::NOT_A_FILE:
        *out = base::File::FILE_ERROR_NOT_A_FILE;
        return true;
      case mojo_base::mojom::FileError::NOT_EMPTY:
        *out = base::File::FILE_ERROR_NOT_EMPTY;
        return true;
      case mojo_base::mojom::FileError::INVALID_URL:
        *out = base::File::FILE_ERROR_INVALID_URL;
        return true;
      case mojo_base::mojom::FileError::IO:
        *out = base::File::FILE_ERROR_IO;
        return true;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FILE_ERROR_MOJOM_TRAITS_H_
