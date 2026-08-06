// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

void Pad(LChar ch,
         wtf_size_t width,
         wtf_size_t value_length,
         StringBuilder& builder) {
  if (width > value_length) {
    for (wtf_size_t k = 0; k < width - value_length; ++k) {
      builder.Append(ch);
    }
  }
}

}  // namespace

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
      } else if (i + 1 < len && (UNSAFE_BUFFERS(format[i + 1]) == '}' ||
                                 UNSAFE_BUFFERS(format[i + 1]) == ':')) {
        uint32_t width = 0;
        // SAFETY: `i + 1` is checked against `len`.
        if (UNSAFE_BUFFERS(format[i + 1]) == ':') {
          auto parsed = internal::ParseWidth(format, i + 2);
          CHECK(parsed.has_value()) << "Format string width out of bounds";
          width = parsed->width;
          i = static_cast<wtf_size_t>(parsed->next_index);
          CHECK_LT(i, len);
          // SAFETY: `i` is checked against `len` via CHECK_LT.
          CHECK_EQ(UNSAFE_BUFFERS(format[i]), '}');
        } else {
          ++i;
        }

        if (arg_index < args.size()) {
          const FormatArg& arg = args[arg_index++];
          std::visit(
              [&builder, width](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int64_t> ||
                              std::is_same_v<T, uint64_t>) {
                  String num_str = String::Number(val);
                  Pad(' ', width, num_str.length(), builder);
                  builder.Append(num_str);
                } else if constexpr (std::is_same_v<T, StringView>) {
                  builder.Append(val);
                  Pad(' ', width, val.length(), builder);
                }
              },
              arg.GetValue());
        }
      } else {
        CHECK(false) << "Invalid format specifier";
      }
    } else if (c == '}') {
      // SAFETY: `i + 1` is checked against `len`.
      if (i + 1 < len && UNSAFE_BUFFERS(format[i + 1]) == '}') {
        builder.Append('}');
        ++i;
      } else {
        CHECK(false) << "Unmatched closing brace";
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
