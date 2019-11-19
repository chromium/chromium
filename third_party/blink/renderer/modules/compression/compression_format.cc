// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/compression/compression_format.h"

#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace blink {

CompressionFormat LookupCompressionFormat(const AtomicString& format,
                                          ExceptionState& exception_state) {
  if (format == "gzip") {
    return CompressionFormat::kGzip;
  } else if (format == "deflate") {
    return CompressionFormat::kDeflate;
  }

  exception_state.ThrowTypeError("Unsupported compression format: '" + format +
                                 "'");
  return CompressionFormat::kGzip;
}

}  // namespace blink
