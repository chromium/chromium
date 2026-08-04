// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <variant>

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

StringBuilder& VFormatTo(StringBuilder& builder,
                         const StringView& format,
                         FormatArgs args) {
  size_t arg_index = 0;
  wtf_size_t len = format.length();

  for (wtf_size_t i = 0; i < len; ++i) {
    // SAFETY: `i` is checked against `len`.
    UChar c = UNSAFE_BUFFERS(format[i]);
    if (c == '{') {
      // SAFETY: `i + 1` is checked against `len`.
      if (i + 1 < len && UNSAFE_BUFFERS(format[i + 1]) == '{') {
        builder.Append('{');
        ++i;
        // SAFETY: `i + 1` is checked against `len`.
      } else if (i + 1 < len && UNSAFE_BUFFERS(format[i + 1]) == '}') {
        if (arg_index < args.size()) {
          const FormatArg& arg = args[arg_index++];
          std::visit(
              [&builder](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int64_t> ||
                              std::is_same_v<T, uint64_t>) {
                  builder.AppendNumber(val);
                } else if constexpr (std::is_same_v<T, StringView>) {
                  builder.Append(val);
                }
              },
              arg.GetValue());
        }
        ++i;
      }
    } else if (c == '}') {
      // SAFETY: `i + 1` is checked against `len`.
      if (i + 1 < len && UNSAFE_BUFFERS(format[i + 1]) == '}') {
        builder.Append('}');
        ++i;
      }
    } else {
      builder.Append(c);
    }
  }
  return builder;
}

String VFormat(const StringView& format, FormatArgs args) {
  StringBuilder builder;
  VFormatTo(builder, format, args);
  return builder.ReleaseString();
}

}  // namespace blink
