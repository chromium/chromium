// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/third_party/double_conversion/double-conversion/double-conversion.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
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

void FormatPointer(const void* ptr,
                   bool is_uppercase,
                   bool zero_pad,
                   uint32_t width,
                   StringBuilder& builder) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  auto append_ptr = [&](auto conv) {
    StringView hex_str(conv.Span());
    wtf_size_t total_len = 2 + hex_str.length();
    const char* prefix = is_uppercase ? "0X" : "0x";
    if (zero_pad) {
      builder.Append(prefix);
      Pad('0', width, total_len, builder);
      builder.Append(hex_str);
    } else {
      Pad(' ', width, total_len, builder);
      builder.Append(prefix);
      builder.Append(hex_str);
    }
  };

  if (is_uppercase) {
    append_ptr(IntegerToStringConverter<uintptr_t, 16, true>(addr));
  } else {
    append_ptr(IntegerToStringConverter<uintptr_t, 16, false>(addr));
  }
}

void FormatDouble(double val,
                  char type,
                  bool zero_pad,
                  uint32_t width,
                  std::optional<uint32_t> precision,
                  StringBuilder& builder) {
  // std::to_chars() is not yet approved for use in Chromium, so
  // we use double_conversion::DoubleToStringConverter instead.
  using D2SConverter = double_conversion::DoubleToStringConverter;
  int flags = D2SConverter::EMIT_POSITIVE_EXPONENT_SIGN;
  if (type == 'g' || type == 'G' || type == '\0') {
    flags |= D2SConverter::NO_TRAILING_ZERO;
  }
  // The last argument is min_exponent_width. printf uses 2.
  D2SConverter converter(flags, "inf", "nan", 'e', -4, 12, 6, 0, 2);
  char buffer[DoubleToStringConverter::kBufferSize];
  double_conversion::StringBuilder dc_builder(buffer, sizeof(buffer));

  bool success = false;
  if (type == 'e' || type == 'E') {
    success = converter.ToExponential(val, precision.value_or(-1), &dc_builder);
  } else if (type == 'f' || type == 'F') {
    success = converter.ToFixed(val, precision.value_or(6), &dc_builder);
  } else {
    if (precision.has_value()) {
      if (precision.value() == 0) {
        // For 'g' and 'G' formatting (which use ToPrecision), the precision
        // represents the number of significant digits. A precision of 0 is
        // treated as 1 by printf-like functions. Also, double_conversion's
        // ToPrecision requires at least 1 digit (kMinPrecisionDigits).
        precision = 1;
      }
      success = converter.ToPrecision(val, precision.value(), &dc_builder);
    } else {
      success = converter.ToShortest(val, &dc_builder);
    }
  }
  CHECK(success) << "double_conversion failed";

  wtf_size_t value_len = static_cast<wtf_size_t>(dc_builder.position());
  auto byte_span = base::as_writable_bytes(base::span(buffer));
  if (type == 'E' || type == 'F' || type == 'G') {
    for (wtf_size_t i = 0; i < value_len; ++i) {
      byte_span[i] = ToAsciiUpper(byte_span[i]);
    }
  }

  bool starts_with_minus = (value_len > 0 && byte_span[0] == '-');
  if (zero_pad) {
    if (starts_with_minus) {
      builder.Append('-');
      Pad('0', width, value_len, builder);
      builder.Append(byte_span.subspan(1u, value_len - 1u));
    } else {
      Pad('0', width, value_len, builder);
      builder.Append(byte_span.first(value_len));
    }
  } else {
    Pad(' ', width, value_len, builder);
    builder.Append(byte_span.first(value_len));
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
        std::optional<uint32_t> precision;
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
          precision = parsed->precision;
          type = parsed->type;
          i = static_cast<wtf_size_t>(parsed->next_index);
          CHECK_LT(i, len);
          // SAFETY: `i` is checked against `len` via CHECK_LT.
          CHECK_EQ(UNSAFE_BUFFERS(format[i]), '}');
        }

        if (arg_index < args.size()) {
          const FormatArg& arg = args[arg_index++];
          std::visit(
              [&builder, width, zero_pad, precision, type](const auto& val) {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, int64_t>) {
                  CHECK(!precision.has_value())
                      << "Precision specified for non-floating-point type";
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
                  CHECK(!precision.has_value())
                      << "Precision specified for non-floating-point type";
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
                } else if constexpr (std::is_same_v<T, double>) {
                  CHECK(type == '\0' || type == 'e' || type == 'E' ||
                        type == 'f' || type == 'F' || type == 'g' ||
                        type == 'G')
                      << "Invalid type specifier for double argument";
                  FormatDouble(val, type, zero_pad, width, precision, builder);
                } else if constexpr (std::is_same_v<T, StringView>) {
                  CHECK(!precision.has_value())
                      << "Precision specified for non-floating-point type";
                  CHECK(type == '\0' || type == 's')
                      << "Invalid type specifier for string argument";
                  builder.Append(val);
                  Pad(' ', width, val.length(), builder);
                } else if constexpr (std::is_same_v<T, const void*>) {
                  CHECK(!precision.has_value())
                      << "Precision specified for non-floating-point type";
                  CHECK(type == '\0' || type == 'p' || type == 'P')
                      << "Invalid type specifier for pointer argument";
                  FormatPointer(val, type == 'P', zero_pad, width, builder);
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
