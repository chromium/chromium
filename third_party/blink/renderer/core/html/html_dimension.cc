/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/html_dimension.h"

#include "third_party/blink/renderer/core/css/css_value_clamping_utils.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/parsing_utilities.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

template <typename CharacterType>
static HTMLDimension ParseDimension(
    base::span<const CharacterType> characters) {
  // HTML5's split removes leading and trailing spaces so we need to skip the
  // leading spaces here.
  const size_t digits_start =
      SkipWhile<CharacterType, IsASCIISpace>(characters, 0);

  // This is Step 5.5. in the algorithm. Going to the last step would make the
  // code less readable.
  if (digits_start >= characters.size()) {
    return HTMLDimension(0., HTMLDimension::kRelative);
  }

  size_t position =
      SkipWhile<CharacterType, IsASCIIDigit>(characters, digits_start);

  double value = 0.;
  if (position > digits_start) {
    bool ok = false;
    unsigned integer_value = CharactersToUInt(
        characters.subspan(digits_start, position - digits_start),
        NumberParsingOptions(), &ok);
    if (!ok)
      return HTMLDimension(0., HTMLDimension::kRelative);
    value += integer_value;

    if (SkipExactly<CharacterType>(characters, '.', position)) {
      Vector<CharacterType> fraction_numbers;
      while (position < characters.size() &&
             (IsASCIIDigit(characters[position]) ||
              IsASCIISpace(characters[position]))) {
        if (IsASCIIDigit(characters[position]))
          fraction_numbers.push_back(characters[position]);
        ++position;
      }

      if (fraction_numbers.size()) {
        double fraction_value = CharactersToUInt(base::span(fraction_numbers),
                                                 NumberParsingOptions(), &ok);
        if (!ok)
          return HTMLDimension(0., HTMLDimension::kRelative);

        value += fraction_value /
                 pow(10., static_cast<double>(fraction_numbers.size()));
      }
    }
  }

  position = SkipWhile<CharacterType, IsASCIISpace>(characters, position);

  HTMLDimension::HTMLDimensionType type = HTMLDimension::kAbsolute;
  if (position < characters.size()) {
    if (characters[position] == '*')
      type = HTMLDimension::kRelative;
    else if (characters[position] == '%')
      type = HTMLDimension::kPercentage;
  }

  return HTMLDimension(value, type);
}

static HTMLDimension ParseDimension(const String& raw_token,
                                    size_t last_parsed_index,
                                    size_t end_of_current_token) {
  return VisitCharacters(
      raw_token, [last_parsed_index, end_of_current_token](auto chars) {
        return ParseDimension(chars.subspan(
            last_parsed_index, end_of_current_token - last_parsed_index));
      });
}

// This implements the "rules for parsing a list of dimensions" per HTML5.
// http://www.whatwg.org/specs/web-apps/current-work/multipage/common-microsyntaxes.html#rules-for-parsing-a-list-of-dimensions
Vector<HTMLDimension> ParseListOfDimensions(const String& input) {
  static const char kComma = ',';

  // Step 2. Remove the last character if it's a comma.
  String trimmed_string = input;
  if (trimmed_string.EndsWith(kComma))
    trimmed_string.Truncate(trimmed_string.length() - 1);

  // HTML5's split doesn't return a token for an empty string so
  // we need to match them here.
  if (trimmed_string.empty())
    return Vector<HTMLDimension>();

  // Step 3. To avoid String copies, we just look for commas instead of
  // splitting.
  Vector<HTMLDimension> parsed_dimensions;
  wtf_size_t last_parsed_index = 0;
  while (true) {
    wtf_size_t next_comma = trimmed_string.find(kComma, last_parsed_index);
    if (next_comma == kNotFound)
      break;

    parsed_dimensions.push_back(
        ParseDimension(trimmed_string, last_parsed_index, next_comma));
    last_parsed_index = next_comma + 1;
  }

  parsed_dimensions.push_back(ParseDimension(trimmed_string, last_parsed_index,
                                             trimmed_string.length()));
  return parsed_dimensions;
}

template <typename CharacterType>
static bool ParseDimensionValue(base::span<const CharacterType> characters,
                                HTMLDimension& dimension) {
  size_t current = SkipWhile<CharacterType, IsHTMLSpace>(characters, 0);
  // Deviation: HTML allows '+' here.
  const size_t number_start = current;
  if (!SkipExactly<CharacterType, IsASCIIDigit>(characters, current)) {
    return false;
  }
  current = SkipWhile<CharacterType, IsASCIIDigit>(characters, current);
  if (SkipExactly<CharacterType>(characters, '.', current)) {
    // Deviation: HTML requires a digit after the full stop to be able to treat
    // the value as a percentage (if not, the '.' will considered "garbage",
    // yielding a regular length.) Gecko and Edge does not.
    current = SkipWhile<CharacterType, IsASCIIDigit>(characters, current);
  }
  bool ok;
  double value = CSSValueClampingUtils::ClampDouble(CharactersToDouble(
      characters.subspan(number_start,
                         static_cast<size_t>(current - number_start)),
      &ok));
  if (!ok)
    return false;
  HTMLDimension::HTMLDimensionType type = HTMLDimension::kAbsolute;
  if (current < characters.size()) {
    const auto c = characters[current];
    if (c == '%') {
      type = HTMLDimension::kPercentage;
    } else if (c == '*') {
      // Deviation: HTML does not recognize '*' in this context, and we don't
      // treat it as a valid value. We do count it though, so this is purely
      // for statistics. Note though that per the specced behavior, "<number>*"
      // would be the same as "<number>" (i.e '*' would just be trailing
      // garbage.)
      type = HTMLDimension::kRelative;
    }
  }
  dimension = HTMLDimension(value, type);
  return true;
}

// https://html.spec.whatwg.org/C/#rules-for-parsing-dimension-values
bool ParseDimensionValue(const String& input, HTMLDimension& dimension) {
  if (input.empty()) {
    return false;
  }
  return VisitCharacters(input, [&dimension](auto chars) {
    return ParseDimensionValue(chars, dimension);
  });
}

}  // namespace blink
