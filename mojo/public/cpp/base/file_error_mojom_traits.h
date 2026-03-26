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

  static base::File::Error FromMojom(mojo_base::mojom::FileError in) {
    switch (in) {
      case mojo_base::mojom::FileError::OK:
        return base::File::FILE_OK;
      case mojo_base::mojom::FileError::FAILED:
        return base::File::FILE_ERROR_FAILED;
      case mojo_base::mojom::FileError::IN_USE:
        return base::File::FILE_ERROR_IN_USE;
      case mojo_base::mojom::FileError::EXISTS:
        return base::File::FILE_ERROR_EXISTS;
      case mojo_base::mojom::FileError::NOT_FOUND:
        return base::File::FILE_ERROR_NOT_FOUND;
      case mojo_base::mojom::FileError::ACCESS_DENIED:
        return base::File::FILE_ERROR_ACCESS_DENIED;
      case mojo_base::mojom::FileError::TOO_MANY_OPENED:
        return base::File::FILE_ERROR_TOO_MANY_OPENED;
      case mojo_base::mojom::FileError::NO_MEMORY:
        return base::File::FILE_ERROR_NO_MEMORY;
      case mojo_base::mojom::FileError::NO_SPACE:
        return base::File::FILE_ERROR_NO_SPACE;
      case mojo_base::mojom::FileError::NOT_A_DIRECTORY:
        return base::File::FILE_ERROR_NOT_A_DIRECTORY;
      case mojo_base::mojom::FileError::INVALID_OPERATION:
        return base::File::FILE_ERROR_INVALID_OPERATION;
      case mojo_base::mojom::FileError::SECURITY:
        return base::File::FILE_ERROR_SECURITY;
      case mojo_base::mojom::FileError::ABORT:
        return base::File::FILE_ERROR_ABORT;
      case mojo_base::mojom::FileError::NOT_A_FILE:
        return base::File::FILE_ERROR_NOT_A_FILE;
      case mojo_base::mojom::FileError::NOT_EMPTY:
        return base::File::FILE_ERROR_NOT_EMPTY;
      case mojo_base::mojom::FileError::INVALID_URL:
        return base::File::FILE_ERROR_INVALID_URL;
      case mojo_base::mojom::FileError::IO:
        return base::File::FILE_ERROR_IO;
    }
    NOTREACHED();
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_FILE_ERROR_MOJOM_TRAITS_H_
