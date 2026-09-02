// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/format.h"

#include <type_traits>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/numerics/checked_math.h"
#include "base/strings/span_printf.h"
#include "base/third_party/double_conversion/double-conversion/double-conversion.h"
#include "third_party/blink/renderer/platform/wtf/dtoa.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/integer_to_string_conversion.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

// Represents formatting options parsed from a replacement field specification
// (e.g. "{:08.2f}") in the format string.
struct FormatSpec {
  std::optional<uint32_t> width;
  std::optional<uint32_t> precision;
  char type = '\0';
  bool is_zero_pad = false;
};

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

void PadAndAppend(base::span<uint8_t> byte_span,
                  bool is_uppercase,
                  bool zero_pad,
                  uint32_t width,
                  StringBuilder& builder) {
  if (is_uppercase) {
    for (uint8_t& byte : byte_span) {
      byte = ToAsciiUpper(byte);
    }
  }
  wtf_size_t value_len = static_cast<wtf_size_t>(byte_span.size());
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

void FormatUnsignedInteger(uint64_t val,
                           char type,
                           bool zero_pad,
                           uint32_t width,
                           StringBuilder& builder) {
  if (type == 'x' || type == 'X') {
    FormatHex(val, /*is_negative=*/false, type == 'X', zero_pad, width,
              builder);
  } else if (type == 'd' || type == '\0') {
    String num_str = String::Number(val);
    Pad(zero_pad ? '0' : ' ', width, num_str.length(), builder);
    builder.Append(num_str);
  } else {
    NOTREACHED() << "Invalid type specifier for unsigned integer argument";
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
  // std::to_chars() is not yet approved for use in Chromium, so we use
  // base::SpanPrintf() and double_conversion::DoubleToStringConverter instead.

  if (type == 'f' || type == 'F') {
    // DoubleToStringConverter::ToFixed has digit limits: it fails when
    // `val` >= 1e60 (kMaxFixedDigitsBeforePoint = 60) or `precision` > 100
    // (kMaxFixedDigitsAfterPoint = 100). Therefore, we use base::SpanPrintf
    // directly for 'f' and 'F' format specifiers.

    // Blink code only specifies fixed precisions in format strings. A buffer
    // of this size can support precision <= kMaxPrecision, which is sufficient
    // for now. Specifying a larger precision will safely crash via CHECK().
    // If such a large precision is needed in the future, we should dynamically
    // allocate a buffer based on the return value of base::SpanPrintf().
    constexpr size_t kMaxDoubleIntegerDigits = 309;
    constexpr size_t kMaxPrecision = 200;
    // 1 (sign) + 309 (integer) + 1 ('.') + 200 (precision) + 1 ('\0') = 512
    char buffer[1 + kMaxDoubleIntegerDigits + 1 + kMaxPrecision + 1];

    uint32_t prec = precision.value_or(6);
    int len = base::SpanPrintf(base::span(buffer), "%.*f", prec, val);
    CHECK_GT(len, 0);
    CHECK_GT(sizeof(buffer), static_cast<size_t>(len));
    auto writable_span = base::as_writable_bytes(base::span(buffer))
                             .first(static_cast<size_t>(len));
    PadAndAppend(writable_span, type == 'F', zero_pad, width, builder);
    return;
  }

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
  } else if (type == 'g' || type == 'G') {
    uint32_t prec = precision.value_or(6);
    if (prec == 0) {
      // For 'g' and 'G' formatting (which use ToPrecision), the precision
      // represents the number of significant digits. A precision of 0 is
      // treated as 1 by printf-like functions. Also, double_conversion's
      // ToPrecision requires at least 1 digit (kMinPrecisionDigits).
      prec = 1;
    }
    success = converter.ToPrecision(val, prec, &dc_builder);
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

  auto writable_span =
      base::as_writable_bytes(base::span(buffer))
          .first(static_cast<wtf_size_t>(dc_builder.position()));
  PadAndAppend(writable_span, type == 'E' || type == 'G', zero_pad, width,
               builder);
}

void FormatValue(bool val, const FormatSpec& spec, StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  uint32_t width = spec.width.value_or(0);
  if (spec.type == '\0' || spec.type == 's') {
    StringView str = val ? "true" : "false";
    builder.Append(str);
    Pad(' ', width, str.length(), builder);
  } else if (spec.type == 'd' || spec.type == 'x' || spec.type == 'X') {
    FormatUnsignedInteger(val ? 1 : 0, spec.type, spec.is_zero_pad, width,
                          builder);
  } else {
    NOTREACHED() << "Invalid type specifier for bool argument";
  }
}

void FormatValue(UChar val, const FormatSpec& spec, StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  if (spec.type == '\0' || spec.type == 'c') {
    CHECK(!spec.width.has_value());
    builder.Append(val);
  } else {
    FormatUnsignedInteger(val, spec.type, spec.is_zero_pad,
                          spec.width.value_or(0), builder);
  }
}

void FormatValue(int64_t val, const FormatSpec& spec, StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  if (spec.type == 'c') {
    CHECK(!spec.width.has_value());
    CHECK_GE(val, 0);
    CHECK_LE(val, static_cast<int64_t>(uchar::kMaxCodepoint));
    builder.Append(static_cast<UChar32>(val));
    return;
  }
  uint32_t width = spec.width.value_or(0);
  if (spec.type == 'x' || spec.type == 'X') {
    bool is_negative = val < 0;
    uint64_t abs_val = is_negative ? (0u - static_cast<uint64_t>(val))
                                   : static_cast<uint64_t>(val);
    FormatHex(abs_val, is_negative, spec.type == 'X', spec.is_zero_pad, width,
              builder);
  } else if (spec.type == '\0' || spec.type == 'd') {
    if (val < 0 && spec.is_zero_pad) {
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
      Pad(spec.is_zero_pad ? '0' : ' ', width, num_str.length(), builder);
      builder.Append(num_str);
    }
  } else {
    NOTREACHED() << "Invalid type specifier for integer argument";
  }
}

void FormatValue(uint64_t val, const FormatSpec& spec, StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  if (spec.type == 'c') {
    CHECK(!spec.width.has_value());
    CHECK_LE(val, static_cast<uint64_t>(uchar::kMaxCodepoint));
    builder.Append(static_cast<UChar32>(val));
  } else {
    FormatUnsignedInteger(val, spec.type, spec.is_zero_pad,
                          spec.width.value_or(0), builder);
  }
}

void FormatValue(double val, const FormatSpec& spec, StringBuilder& builder) {
  CHECK(spec.type == '\0' || spec.type == 'e' || spec.type == 'E' ||
        spec.type == 'f' || spec.type == 'F' || spec.type == 'g' ||
        spec.type == 'G')
      << "Invalid type specifier for double argument";
  FormatDouble(val, spec.type, spec.is_zero_pad, spec.width.value_or(0),
               spec.precision, builder);
}

void FormatValue(const StringView& val,
                 const FormatSpec& spec,
                 StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  CHECK(spec.type == '\0' || spec.type == 's')
      << "Invalid type specifier for string argument";
  builder.Append(val);
  Pad(' ', spec.width.value_or(0), val.length(), builder);
}

void FormatValue(const void* val,
                 const FormatSpec& spec,
                 StringBuilder& builder) {
  CHECK(!spec.precision.has_value())
      << "Precision specified for non-floating-point type";
  CHECK(spec.type == '\0' || spec.type == 'p' || spec.type == 'P')
      << "Invalid type specifier for pointer argument";
  FormatPointer(val, spec.type == 'P', spec.is_zero_pad, spec.width.value_or(0),
                builder);
}

// Pre-allocates buffer capacity in `builder` using a rough estimation of the
// formatted string and arguments. This estimation does not account for format
// specifiers like width (e.g. "{:100}"), precision, or brace escapes, but
// provides a fast, reasonable upper bound for typical cases to avoid repeated
// reallocations.
void ReserveEstimatedCapacity(const StringView& format,
                              FormatArgs args,
                              StringBuilder& builder) {
  base::CheckedNumeric<wtf_size_t> estimated_len = builder.length();
  estimated_len += format.length();
  bool is_8bit = format.Is8Bit();

  for (const FormatArg& arg : args) {
    std::visit(
        [&](const auto& val) {
          using T = std::decay_t<decltype(val)>;
          if constexpr (std::is_same_v<T, StringView>) {
            estimated_len += val.length();
            if (!val.Is8Bit()) {
              is_8bit = false;
            }
          } else if constexpr (std::is_same_v<T, int64_t> ||
                               std::is_same_v<T, uint64_t> ||
                               std::is_same_v<T, double> ||
                               std::is_same_v<T, const void*>) {
            // Numbers, pointers, and floats rarely exceed 20 characters in
            // standard formatting.
            estimated_len += 20;
          } else if constexpr (std::is_same_v<T, bool>) {
            // "false" is 5 characters, "true" is 4 characters.
            estimated_len += 5;
          } else if constexpr (std::is_same_v<T, UChar>) {
            // A single character.
            estimated_len += 1;
            if (val > 0xff) {
              is_8bit = false;
            }
          }
        },
        arg.GetValue());
  }

  if (estimated_len.IsValid()) {
    if (is_8bit) {
      builder.ReserveCapacity(estimated_len.ValueOrDie());
    } else {
      builder.Reserve16BitCapacity(estimated_len.ValueOrDie());
    }
  }
}

}  // namespace

StringBuilder& VFormatTo(StringBuilder& builder,
                         const StringView& format,
                         FormatArgs args) {
  ReserveEstimatedCapacity(format, args, builder);

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
        FormatSpec spec;
        // SAFETY: `i` is checked against `len`.
        if (UNSAFE_BUFFERS(format[i]) == ':') {
          ++i;
          // SAFETY: `i` is checked against `len`.
          if (i < len && UNSAFE_BUFFERS(format[i]) == '0') {
            spec.is_zero_pad = true;
            ++i;
          }
          auto parsed = internal::ParseFormatSpec(format, i);
          CHECK(parsed.has_value()) << "Invalid format specifier";
          spec.width = parsed->width;
          spec.precision = parsed->precision;
          spec.type = parsed->type;
          i = static_cast<wtf_size_t>(parsed->next_index);
          CHECK_LT(i, len);
          // SAFETY: `i` is checked against `len` via CHECK_LT.
          CHECK_EQ(UNSAFE_BUFFERS(format[i]), '}');
        }

        if (arg_index < args.size()) {
          const FormatArg& arg = args[arg_index++];
          std::visit([&](const auto& val) { FormatValue(val, spec, builder); },
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
