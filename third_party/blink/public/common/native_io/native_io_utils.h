// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_NATIVE_IO_NATIVE_IO_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_NATIVE_IO_NATIVE_IO_UTILS_H_

#include <cstdint>

#include "base/files/file.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom-shared.h"

namespace blink {
namespace native_io {

BLINK_COMMON_EXPORT blink::mojom::NativeIOErrorType
FileErrorToNativeIOErrorType(const base::File::Error error);

BLINK_COMMON_EXPORT std::string GetDefaultMessage(
    const blink::mojom::NativeIOErrorType nativeio_error);

}  // namespace native_io
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_NATIVE_IO_NATIVE_IO_UTILS_H_
