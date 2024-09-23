// Copyright 2019 The Chromium Authors
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
  } else if (format == "deflate-raw") {
    return CompressionFormat::kDeflateRaw;
  }

  exception_state.ThrowTypeError("Unsupported compression format: '" + format +
                                 "'");
  return CompressionFormat::kGzip;
}

}  // namespace blink
