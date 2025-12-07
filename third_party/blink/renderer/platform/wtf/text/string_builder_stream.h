// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_STREAM_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_STREAM_H_

#include <concepts>

#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Append a Latin-1 string
inline StringBuilder& operator<<(StringBuilder& builder, const char* cstr) {
  builder.Append(StringView(cstr));
  return builder;
}

inline StringBuilder& operator<<(StringBuilder& builder, StringView view) {
  builder.Append(view);
  return builder;
}

// Append a Latin-1 string.
inline StringBuilder& operator<<(StringBuilder& builder,
                                 const std::string& str) {
  builder.Append(base::as_byte_span(str));
  return builder;
}

template <std::integral T>
StringBuilder& operator<<(StringBuilder& builder, T number) {
  builder.AppendNumber(number);
  return builder;
}

template <std::floating_point T>
StringBuilder& operator<<(StringBuilder& builder, T number) {
  builder.AppendNumber(number);
  return builder;
}

// Append the specified `Vector<T>` like "[element0, element1, element2]".
//
// `T` should be supported by StringBuilder::Append() or
// StringBuilder::AppendNumber().
template <typename T>
  requires(IsAppendableType<T>)
StringBuilder& operator<<(StringBuilder& builder, const Vector<T>& vector) {
  builder << "[";
  builder.AppendRange(vector, ", ");
  return builder << "]";
}

// Append index*2 spaces.
WTF_EXPORT void WriteIndent(StringBuilder& builder, wtf_size_t indent);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_STRING_BUILDER_STREAM_H_
