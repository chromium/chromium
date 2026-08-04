// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_

#include <concepts>
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <variant>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

// `blink::Format()`, `blink::VFormat()`, and `blink::VFormatTo()` provide
// string formatting functionality for Blink's WTF string types, inspired by
// C++20's `std::format()`, `std::vformat()`, and `std::vformat_to()`.

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
// formatting. This class is used internally by `Format()`, `VFormat()`, and
// `VFormatTo()` and is not intended for direct public usage.
class WTF_EXPORT FormatArg {
 public:
  using Value = std::variant<int64_t, uint64_t, StringView>;

  // NOLINTBEGIN(google-explicit-constructor)
  FormatArg(int32_t v) : value_(static_cast<int64_t>(v)) {}
  FormatArg(uint32_t v) : value_(static_cast<uint64_t>(v)) {}
  FormatArg(int64_t v) : value_(v) {}
  FormatArg(uint64_t v) : value_(v) {}
  template <typename T>
    requires std::convertible_to<const T&, StringView>
  FormatArg(const T& v) : value_(StringView(v)) {}
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
// - Placeholders: Only unindexed `{}` is supported. Positional (e.g. `{0}`) or
//   typed (e.g. `{:d}`) format specifiers are currently not supported.
// - Escaping: `{{` outputs `{`, and `}}` outputs `}`.
//
// Supported Argument Types:
// - Integral types: `int32_t`, `uint32_t`, `int64_t`, `uint64_t` (and
//   implicitly convertible types).
// - String types: `blink::StringView`, `blink::String`, `blink::AtomicString`,
//   `const char[N]`.
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
//   // Escaping braces:
//   String escaped = blink::Format("{{ {} }}", 42);
//   // ==> "{ 42 }"
//
// Compile-time Validation:
// If the number of `{}` placeholders does not match the number of arguments,
// or if braces are unclosed or unmatched, a compile-time error will be raised.
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_FORMAT_H_
