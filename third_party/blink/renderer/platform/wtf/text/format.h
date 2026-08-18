// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/numerics/checked_math.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// `blink::Format()`, `blink::FormatTo()`, `blink::VFormat()`, and
// `blink::VFormatTo()` provide string formatting functionality for Blink's WTF
// string types, inspired by C++20's `std::format()`, `std::format_to()`,
// `std::vformat()`, and `std::vformat_to()`.

namespace blink {

class StringBuilder;

// Helper function to trigger a compile-time error when format string parsing
// fails. Intentionally left undefined so calling it in a consteval context
// causes a compile error.
consteval void FormatStringError(const char* message) {
  void NonConstexprFormatError();
  NonConstexprFormatError();
}

// Internal helper class representing a single type-erased argument for
// formatting. This class is used internally by `Format()`, `FormatTo()`,
// `VFormat()`, and `VFormatTo()` and is not intended for direct public usage.
class WTF_EXPORT FormatArg {
 public:
  using Value =
      std::variant<int64_t, uint64_t, StringView, const void*, double, UChar>;

  // NOLINTBEGIN(google-explicit-constructor)
  FormatArg(char v)
      : value_(static_cast<UChar>(static_cast<unsigned char>(v))) {}
  FormatArg(LChar v) : value_(static_cast<UChar>(v)) {}
  FormatArg(UChar v) : value_(v) {}
  FormatArg(int v) : value_(static_cast<int64_t>(v)) {}
  FormatArg(unsigned int v) : value_(static_cast<uint64_t>(v)) {}
  FormatArg(long v) : value_(static_cast<int64_t>(v)) {}
  FormatArg(unsigned long v) : value_(static_cast<uint64_t>(v)) {}
  FormatArg(long long v) : value_(v) {}
  FormatArg(unsigned long long v) : value_(v) {}
  FormatArg(const void* v) : value_(v) {}
  FormatArg(std::nullptr_t) : value_(static_cast<const void*>(nullptr)) {}
  FormatArg(double v) : value_(v) {}
  template <typename T>
    requires std::convertible_to<const T&, StringView>
  FormatArg(const T& v) : value_(StringView(v)) {}
  template <typename T>
    requires(!std::convertible_to<const T&, StringView> &&
             std::convertible_to<const T&, std::string_view>)
  FormatArg(const T& v) {
    std::string_view sv = v;
    value_ = StringView(base::as_byte_span(sv));
  }
  // NOLINTEND(google-explicit-constructor)

  FormatArg() = default;
  ~FormatArg() = default;
  FormatArg(const FormatArg&) = default;
  FormatArg& operator=(const FormatArg&) = default;

  const Value& GetValue() const { return value_; }

 private:
  Value value_;
};

// Type alias for a view over a list of type-erased `FormatArg`s.
using FormatArgs = base::span<const FormatArg>;

namespace internal {

struct ParsedInteger {
  uint32_t value = 0;
  size_t next_index = 0;
};

// Common constexpr helper to parse width or precision specifier.
// Returns the parsed integer and next index after digits, or std::nullopt if
// value is out of bounds.
// If no digits are present, returns value 0 and `start_index`.
template <typename StringType>
constexpr std::optional<ParsedInteger> ParseInteger(
    const StringType& format,
    typename StringType::size_type start_index) {
  auto len = format.length();
  if (start_index >= len) {
    return ParsedInteger{0, start_index};
  }
  // SAFETY: `start_index` is checked against `len`.
  auto first_ch = UNSAFE_BUFFERS(format[start_index]);
  if (!IsAsciiDigit(first_ch)) {
    return ParsedInteger{0, start_index};
  }

  base::CheckedNumeric<uint32_t> value = 0;
  auto i = start_index;
  while (i < len) {
    // SAFETY: `i` is checked against `len` in the loop condition.
    auto ch = UNSAFE_BUFFERS(format[i]);
    if (!IsAsciiDigit(ch)) {
      break;
    }
    value = value * 10 + static_cast<uint32_t>(ch - '0');
    if (!value.IsValid()) {
      return std::nullopt;
    }
    ++i;
  }
  return ParsedInteger{value.ValueOrDefault(0), i};
}

struct ParsedFormatSpec {
  std::optional<uint32_t> width;
  std::optional<uint32_t> precision;
  char type = '\0';
  size_t next_index = 0;
};

// Common constexpr helper to parse format specifier
// {:[width][.precision][type]}.
template <typename StringType>
constexpr std::optional<ParsedFormatSpec> ParseFormatSpec(
    const StringType& format,
    typename StringType::size_type start_index) {
  using SizeType = typename StringType::size_type;
  auto width_parsed = ParseInteger(format, start_index);
  if (!width_parsed.has_value()) {
    return std::nullopt;
  }
  std::optional<uint32_t> width;
  if (width_parsed->next_index > start_index) {
    width = width_parsed->value;
  }
  SizeType i = static_cast<SizeType>(width_parsed->next_index);
  auto len = format.length();

  std::optional<uint32_t> precision;
  // SAFETY: `i` is checked against `len`.
  if (i < len && UNSAFE_BUFFERS(format[i]) == '.') {
    ++i;
    auto precision_parsed = ParseInteger(format, i);
    if (!precision_parsed.has_value() || precision_parsed->next_index == i) {
      return std::nullopt;
    }
    precision = precision_parsed->value;
    i = static_cast<SizeType>(precision_parsed->next_index);
  }

  char type = '\0';

  if (i < len) {
    // SAFETY: `i` is checked against `len`.
    auto ch = UNSAFE_BUFFERS(format[i]);
    if (IsAsciiAlpha(ch)) {
      type = static_cast<char>(ch);
      ++i;
    }
  }

  if (type != '\0' && type != 'd' && type != 'x' && type != 'X' &&
      type != 's' && type != 'p' && type != 'P' && type != 'e' && type != 'E' &&
      type != 'f' && type != 'F' && type != 'g' && type != 'G' && type != 'c') {
    return std::nullopt;
  }

  // SAFETY: `i` is checked against `len`.
  if (i >= len || UNSAFE_BUFFERS(format[i]) != '}') {
    return std::nullopt;
  }

  return ParsedFormatSpec{
      .width = width, .precision = precision, .type = type, .next_index = i};
}

}  // namespace internal

// Internal wrapper class for format strings that performs compile-time
// validation. Checks that braces `{}` match the number of arguments `Args...`
// and validates escape sequences `{{` and `}}`. Not intended for direct public
// usage.
template <typename... Args>
class FormatString {
 public:
  template <typename T>
    requires std::convertible_to<const T&, std::string_view>
  consteval FormatString(const T& s)  // NOLINT(google-explicit-constructor)
      : format_(s) {
    size_t brace_count = 0;
    size_t len = format_.length();
    for (size_t i = 0; i < len; ++i) {
      if (format_[i] == '{') {
        if (i + 1 < len && format_[i + 1] == '{') {
          ++i;
        } else if (i + 1 < len && format_[i + 1] == '}') {
          ++brace_count;
          ++i;
        } else if (i + 1 < len && format_[i + 1] == ':') {
          auto parsed = internal::ParseFormatSpec(format_, i + 2);
          if (!parsed.has_value()) {
            FormatStringError(
                "Invalid format string: invalid format specifier");
          }
          if (parsed->width.has_value() || parsed->precision.has_value() ||
              parsed->type != '\0') {
            if (!CheckArgTypeAtIndex(brace_count, parsed->type,
                                     parsed->width.has_value(),
                                     parsed->precision.has_value())) {
              FormatStringError(
                  "Invalid format string: argument type mismatch for type "
                  "specifier or precision");
            }
          }
          i = parsed->next_index;
          ++brace_count;
        } else {
          FormatStringError(
              "Invalid format string: unclosed or unsupported brace specifier");
        }
      } else if (format_[i] == '}') {
        if (i + 1 < len && format_[i + 1] == '}') {
          ++i;
        } else {
          FormatStringError("Invalid format string: unmatched closing brace");
        }
      }
    }

    if (brace_count != sizeof...(Args)) {
      FormatStringError("Format string argument count mismatch");
    }
  }

  StringView GetStringView() const {
    return StringView(base::as_bytes(base::span(format_)));
  }

 private:
  static consteval bool CheckArgTypeAtIndex(size_t index,
                                            char type,
                                            bool has_width,
                                            bool has_precision) {
    size_t current = 0;
    bool valid = true;
    auto check = [&](auto dummy) {
      using RawT = std::remove_cvref_t<typename decltype(dummy)::type>;
      if (current == index) {
        bool is_uchar = std::is_same_v<RawT, char> ||
                        std::is_same_v<RawT, LChar> ||
                        std::is_same_v<RawT, UChar>;

        if (type == 'd' || type == 'x' || type == 'X') {
          valid = std::is_integral_v<RawT> || std::is_enum_v<RawT>;
        } else if (type == 'c') {
          valid = std::is_integral_v<RawT> || std::is_enum_v<RawT>;
        } else if (type == 's') {
          valid = std::convertible_to<const RawT&, StringView> ||
                  std::convertible_to<const RawT&, std::string_view>;
        } else if (type == 'p' || type == 'P') {
          valid = (std::convertible_to<RawT, const void*> &&
                   !std::convertible_to<const RawT&, StringView> &&
                   !std::convertible_to<const RawT&, std::string_view>) ||
                  std::is_same_v<RawT, std::nullptr_t>;
        } else if (type == 'e' || type == 'E' || type == 'f' || type == 'F' ||
                   type == 'g' || type == 'G') {
          valid = std::is_floating_point_v<RawT>;
        }

        if (type == 'c' || (is_uchar && type == '\0')) {
          if (has_width || has_precision) {
            valid = false;
          }
        }

        if (has_precision) {
          if (!std::is_floating_point_v<RawT>) {
            valid = false;
          }
          if (type != '\0' && type != 'e' && type != 'E' && type != 'f' &&
              type != 'F' && type != 'g' && type != 'G') {
            valid = false;
          }
        }
      }
      current++;
    };
    (check(std::type_identity<Args>{}), ...);
    return valid;
  }

  std::string_view format_;
};

// Formats a string using a type-erased argument list (`FormatArgs`).
// Inspired by C++20's `std::vformat()`.
//
// Parameters:
// - `format`: A `StringView` containing the format string. Supports `{}`
//   placeholders and `{{` / `}}` escape sequences.
// - `args`: Type-erased arguments wrapped in a `FormatArgs` instance.
//
// Returns:
// - A `blink::String` with all placeholders replaced by formatted argument
//   values.
//
// Note: Prefer using `blink::Format()` for compile-time validation of format
// strings.
WTF_EXPORT String VFormat(const StringView& format, FormatArgs args);

// Appends a formatted string to a `StringBuilder` using a type-erased argument
// list (`FormatArgs`). Inspired by C++20's `std::vformat_to()`.
//
// Parameters:
// - `builder`: The `StringBuilder` to append the formatted result to.
// - `format`: A `StringView` containing the format string. Supports `{}`
//   placeholders and `{{` / `}}` escape sequences.
// - `args`: Type-erased arguments wrapped in a `FormatArgs` instance.
//
// Return value:
//   `builder` is returned for chaining.
WTF_EXPORT StringBuilder& VFormatTo(StringBuilder& builder,
                                    const StringView& format,
                                    FormatArgs args);

// Formats a string with compile-time format string validation and argument
// count checking. Inspired by C++20's `std::format()`.
//
// Format String Specifications:
// - Encoding: Expects ASCII / Latin1 string literals or `std::string_view`
//   convertible types.
// - Placeholders: Unindexed `{}` or `{:}` and width-specified `{:width}` or
//   zero-padded `{:0width}` (where width is a 32-bit unsigned integer) with
//   an optional precision specifier `:.precision` (for floating-point types
//   only), and an optional type specifier `c`, `d`, `x`, `X`, `s`, `p`, `P`,
//   `e`, `E`, `f`, `F`, `g`, `G` (e.g. `{:d}`, `{:08x}`, `{:p}`, `{:.2f}`,
//   `{:c}`) are supported.
//   Positional (e.g. `{0}`) format specifiers are currently not supported.
//   Width and precision are forbidden for the `c` type specifier and for
//   character types without a type specifier.
// - Escaping: `{{` outputs `{`, and `}}` outputs `}`.
//
// Supported Argument Types:
// - Character types: `char`, `LChar`, `UChar`. If no type specifier is given,
//   they are formatted as characters. They can also be formatted as integers
//   using `d`, `x`, `X`.
// - Integral types: `int32_t`, `uint32_t`, `int64_t`, `uint64_t` (and
//   implicitly convertible types). The `c` specifier formats integral types as
//   characters. A CHECK failure occurs if the value is negative or exceeds
//   0x10FFFF.
// - Floating-point types: `double` (and implicitly convertible types).
// - String types: `blink::StringView`, `blink::String`, `blink::AtomicString`,
//   `std::string`, `std::string_view`, `const char[N]`.
//   Note that `std::string`, `std::string_view`, and `const char[N]` are
//   treated as Latin-1, not UTF-8.
// - Pointer types: `const void*`, `std::nullptr_t` (and implicitly
//   convertible types).
//
// Usage Examples:
//   // Basic formatting:
//   String msg = blink::Format("Hello {}", StringView("world"));
//   // ==> "Hello world"
//
//   // Formatting multiple arguments:
//   String sum = blink::Format("{} + {} = {}", 1, 2, 3);
//   // ==> "1 + 2 = 3"
//
//   // Precision for floating-point values:
//   String pi = blink::Format("{:.2f}", 3.14159);
//   // ==> "3.14"
//
//   // Escaping braces:
//   String escaped = blink::Format("{{ {} }}", 42);
//   // ==> "{ 42 }"
//
// Compile-time Validation:
// If the number of `{}` placeholders does not match the number of arguments,
// or if braces are unclosed or unmatched, a compile-time error will be raised.
// Specifying precision for non-floating-point types will also raise an error.
template <typename... Args>
inline String Format(FormatString<std::type_identity_t<Args>...> format,
                     Args&&... args) {
  if constexpr (sizeof...(Args) == 0) {
    return VFormat(format.GetStringView(), FormatArgs());
  } else {
    const FormatArg arg_array[] = {FormatArg(args)...};
    return VFormat(format.GetStringView(), base::span(arg_array));
  }
}

// Appends a formatted string to a `StringBuilder` with compile-time format
// string validation and argument count checking. Inspired by C++20's
// `std::format_to()`.
//
// Parameters:
// - `builder`: The `StringBuilder` to append the formatted result to.
// - `format`: A format string with compile-time validation. See `Format()` for
//   specifications and supported types.
// - `args`: The arguments to format.
//
// Return value:
//   `builder` is returned for chaining.
template <typename... Args>
inline StringBuilder& FormatTo(
    StringBuilder& builder,
    FormatString<std::type_identity_t<Args>...> format,
    Args&&... args) {
  if constexpr (sizeof...(Args) == 0) {
    return VFormatTo(builder, format.GetStringView(), FormatArgs());
  } else {
    const FormatArg arg_array[] = {FormatArg(args)...};
    return VFormatTo(builder, format.GetStringView(), base::span(arg_array));
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_
