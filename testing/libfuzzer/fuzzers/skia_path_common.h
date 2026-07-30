// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_LIBFUZZER_FUZZERS_SKIA_PATH_COMMON_H_
#define TESTING_LIBFUZZER_FUZZERS_SKIA_PATH_COMMON_H_

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "third_party/skia/include/core/SkPath.h"

// Helper functions to read values from a SpanReader.
//
// The single-value read() has two overloads:
// 1. For types that can be safely converted to a byte span (unique object
//    representation, no padding).
template <typename T>
  requires(base::kCanSafelyConvertToByteSpan<T>)
static bool read(base::SpanReader<const uint8_t>& reader, T* value) {
  return reader.ReadCopy(base::as_writable_bytes(base::span_from_ref(*value)));
}

// 2. For types that might have padding bytes, requiring
//    base::allow_nonunique_obj to allow writing to them via span.
template <typename T>
  requires(!base::kCanSafelyConvertToByteSpan<T>)
static bool read(base::SpanReader<const uint8_t>& reader, T* value) {
  return reader.ReadCopy(base::as_writable_bytes(base::allow_nonunique_obj,
                                                 base::span_from_ref(*value)));
}

// The variadic read() allows reading multiple values in a single call.
template <typename T1, typename T2, typename... Args>
static bool read(base::SpanReader<const uint8_t>& reader,
                 T1* val1,
                 T2* val2,
                 Args*... args) {
  return read(reader, val1) && read(reader, val2, args...);
}

SkPath BuildPath(base::SpanReader<const uint8_t>& reader, int last_verb);

#endif  // TESTING_LIBFUZZER_FUZZERS_SKIA_PATH_COMMON_H_
