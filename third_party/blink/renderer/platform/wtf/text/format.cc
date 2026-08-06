// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
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

void FormatHex(uint64_t abs_val,
               bool is_negative,
               bool is_uppercase,
               bool zero_pad,
               uint32_t width,
               StringBuilder& builder) {
  auto append_hex = [&](auto conv) {
    StringView hex_str(conv.Span());
    if (is_negative) {
      if (zero_pad && width > hex_str.length() + 1) {
        builder.Append('-');
        Pad('0', width, hex_str.length() + 1, builder);
        builder.Append(hex_str);
      } else {
        Pad(' ', width, hex_str.length() + 1, builder);
        builder.Append('-');
        builder.Append(hex_str);
      }
    } else {
      Pad(zero_pad ? '0' : ' ', width, hex_str.length(), builder);
      builder.Append(hex_str);
    }
  };

  if (is_uppercase) {
    append_hex(IntegerToStringConverter<uint64_t, 16, true>(abs_val));
  } else {
    append_hex(IntegerToStringConverter<uint64_t, 16, false>(abs_val));
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
        ++i;
        uint32_t width = 0;
        bool zero_pad = false;
        char type = '\0';
        // SAFETY: `i` is checked against `len`.
        if (UNSAFE_BUFFERS(format[i]) == ':') {
          ++i;
          // SAFETY: `i` is checked against `len`.
          if (i < len && UNSAFE_BUFFERS(format[i]) == '0') {
            zero_pad = true;
            ++i;
          }
          auto parsed = internal::ParseFormatSpec(format, i);
          CHECK(parsed.has_value()) << "Invalid format specifier";
          width = parsed->width;
          type = parsed->type;
          i = static_cast<wtf_size_t>(parsed->next_index);
          CHECK_LT(i, len);
          // SAFETY: `i` is checked against `len` via CHECK_LT.
          CHECK_EQ(UNSAFE_BUFFERS(format[i]), '}');
        }

        if (arg_index < args.size()) {
          const FormatArg& arg = args[arg_index++];
          std::visit(
              [&builder, width, zero_pad, type](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                  CHECK(type == '\0' || type == 'd' || type == 'x' ||
                        type == 'X')
                      << "Invalid type specifier for integer argument";
                  if (type == 'x' || type == 'X') {
                    bool is_negative = val < 0;
                    uint64_t abs_val =
                        is_negative ? (0u - static_cast<uint64_t>(val))
                                    : static_cast<uint64_t>(val);
                    FormatHex(abs_val, is_negative, type == 'X', zero_pad,
                              width, builder);
                  } else {
                    if (val < 0 && zero_pad) {
                      String num_str = String::Number(val);
                      if (width > num_str.length()) {
                        builder.Append('-');
                        Pad('0', width, num_str.length(), builder);
                        builder.Append(StringView(num_str, 1));
                      } else {
                        builder.Append(num_str);
                      }
                    } else {
                      String num_str = String::Number(val);
                      Pad(zero_pad ? '0' : ' ', width, num_str.length(), builder);
                      builder.Append(num_str);
                    }
                  }
                } else if constexpr (std::is_same_v<T, uint64_t>) {
                  CHECK(type == '\0' || type == 'd' || type == 'x' ||
                        type == 'X')
                      << "Invalid type specifier for unsigned integer argument";
                  if (type == 'x' || type == 'X') {
                    FormatHex(val, /*is_negative=*/false, type == 'X', zero_pad,
                              width, builder);
                  } else {
                    String num_str = String::Number(val);
                    Pad(zero_pad ? '0' : ' ', width, num_str.length(), builder);
                    builder.Append(num_str);
                  }
                } else if constexpr (std::is_same_v<T, StringView>) {
                  CHECK(type == '\0' || type == 's')
                      << "Invalid type specifier for string argument";
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
