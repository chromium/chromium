// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_URL_UNESCAPE_ITERATOR_H_
#define NET_BASE_URL_UNESCAPE_ITERATOR_H_

#include <stddef.h>

#include <array>
#include <iterator>
#include <ranges>
#include <string_view>
#include <tuple>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "url/origin.h"

namespace net {

// An iterator that unescapes a URL-encoded std::string_view in exactly the same
// way as UnescapePercentEncodedUrl() but without needing to allocate space for
// the output.
class NET_EXPORT_PRIVATE UrlUnescapeIterator final {
 public:
  using WrappedIterator = std::string_view::const_iterator;

  using difference_type =
      std::iterator_traits<WrappedIterator>::difference_type;
  using value_type = char;
  using reference = char;
  using iterator_category = std::forward_iterator_tag;

  // Only useful when you need to pre-declare the iterator for some reason. Not
  // otherwise valid.
  constexpr UrlUnescapeIterator() = default;

  // It's usually preferable to call MakeUrlUnescapeRange() (below) rather than
  // use this constructor directly.
  constexpr UrlUnescapeIterator(WrappedIterator start, WrappedIterator end)
      : current_(start), next_(start), end_(end) {
    DecodeNext();
  }

  constexpr UrlUnescapeIterator(const UrlUnescapeIterator&) = default;

  constexpr UrlUnescapeIterator& operator=(const UrlUnescapeIterator&) =
      default;

  constexpr bool operator==(const UrlUnescapeIterator& rhs) const {
    // There's no need to compare the other member variables.
    return current_ == rhs.current_ &&
           replacement_character_byte_ == rhs.replacement_character_byte_;
  }

  // Implements ++x.
  constexpr UrlUnescapeIterator& operator++() {
    CHECK(current_ != end_);

    if (replacement_character_byte_) [[unlikely]] {
      IncrementReplacementChar();
      return *this;
    }

    DecodeNext();
    return *this;
  }

  // Implements x++.
  constexpr UrlUnescapeIterator operator++(int) {
    UrlUnescapeIterator previous_value = *this;
    ++*this;
    return previous_value;
  }

  constexpr char operator*() const {
    CHECK(current_ != end_);
    return value_;
  }

 private:
  // The unicode replacement character U+FFFD, encoded as UTF-8. Used to replace
  // invalid UTF-8 in the input.
  static constexpr auto kReplacementCharacterInUTF8 =
      std::to_array<char>({static_cast<char>(0xEF), static_cast<char>(0xBF),
                           static_cast<char>(0xBD)});

  // Advances `current_` and `next_` and sets `value_`.
  constexpr void DecodeNext() {
    current_ = next_;
    if (current_ == end_) {
      value_ = 0;
      return;
    }
    std::tie(value_, next_) = DecodeAt(current_);
    if ((value_ & 0x80) == 0) {
      // ASCII bytes need no further checking.
      return;
    }
    if (remaining_checked_output_bytes_) {
      // `value_` is one of the trailing bytes of a valid UTF-8 character and
      // has already been checked.
      --remaining_checked_output_bytes_;
      return;
    }

    // `value_` is the first byte of a UTF-8 character, or invalid.
    CheckNonAscii();
  }

  // Decodes a single byte at `place`. Returns the decoded byte, and `place`
  // advanced by one or three input bytes.
  constexpr std::pair<char, WrappedIterator> DecodeAt(WrappedIterator place) {
    CHECK(place != end_);
    const auto next = std::next(place);
    if (*place == '+') {
      return {' ', next};
    } else if (*place == '%') {
      return DecodePercent(next);
    } else {
      return {*place, next};
    }
  }

  // Sets `value_` to the `replacement_character_byte_` byte of
  // `kReplacementCharacterInUTF8` and increments
  // `replacement_character_byte_`. After setting `value_` to the last byte of
  // `kReplacementCharacterInUTF8`, sets `replacement_character_byte_` to 0 so
  // that normal iteration can proceed.
  void IncrementReplacementChar();

  // Attempts to decode a %-encoded byte. If `next` and `next + 1` are before
  // `end_` and valid hexadecimal characters, returns the decoded byte and an
  // iterator pointing to `next + 2`. Otherwise, returns '%' and `next`.
  std::pair<char, WrappedIterator> DecodePercent(WrappedIterator next);

  // Checks that `value_`, `*next_` and zero or more following bytes are a
  // well-formed UTF-8 code-point. If they are, sets `prechecked_output_bytes_`
  // to the number of well-formed UTF-8 bytes left to be read, and sets `next_`
  // to the first byte after the UTF-8 code-point. If not, sets `value_` to the
  // first byte of `kReplacementCharacterInUTF8` and
  // `replacement_character_byte_` to 1 so that the other two bytes of
  // `kReplacementCharacterInUTF8` will be output to follow. Sets `next_` to
  // point to the first character that doesn't form part of a valid UTF-8
  // character prefix.
  void CheckNonAscii();

  // Sets `value_` to kReplacementCharacterInUTF8[0] and
  // `replacement_character_byte_` to 1.
  void EmitReplacementCharacter();

  // The current position of the iterator in the underlying string_view.
  WrappedIterator current_{};

  // The start of the input for the next output byte, or the off-the-end
  // iterator. This will be equal to `current_ + 3` if the current output byte
  // was the result of decoding a %-encoded byte.
  WrappedIterator next_{};

  // The off-the-end iterator. As well as safety checks, this is needed to
  // correctly handle '%' characters at the end of the string.
  WrappedIterator end_{};

  // The value that will be returned by `operator*`. This is cached by
  // `operator++`.
  char value_ = 0;

  // If this is non-zero, then `operator++` will set `value_` to the next byte
  // of the UTF-8 replacement character and not advance `current_`.
  size_t replacement_character_byte_ = 0;

  // If this is non-zero, then the next `remaining_checked_output_bytes_` bytes
  // of output have already been checked for UTF-8 validity and should be output
  // without further checking.
  size_t remaining_checked_output_bytes_ = 0;
};

// Returns a range consisting of two UrlUnescapeIterator iterators. Iterating
// over the resulting range will give the decoded bytes.
constexpr std::ranges::subrange<UrlUnescapeIterator> MakeUrlUnescapeRange(
    std::string_view escaped_url_component LIFETIME_BOUND) {
  const auto component_end = escaped_url_component.end();
  const UrlUnescapeIterator start(escaped_url_component.begin(), component_end);
  const UrlUnescapeIterator end(component_end, component_end);
  return {start, end};
}

// Returns true if `a` and `b` would be equal after decoding with
// UrlUnescapeIterator. Optimized to avoid actually performing decoding in
// common cases.
NET_EXPORT_PRIVATE bool EqualsAfterUrlDecoding(std::string_view a,
                                               std::string_view b);

}  // namespace net

#endif  // NET_BASE_URL_UNESCAPE_ITERATOR_H_
