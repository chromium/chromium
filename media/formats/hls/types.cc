// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"

#include <algorithm>
#include <cmath>

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "media/formats/hls/source_string.h"
#include "third_party/re2/src/re2/re2.h"

namespace media::hls::types {

namespace {
re2::StringPiece to_re2(base::StringPiece str) {
  return re2::StringPiece(str.data(), str.size());
}

re2::StringPiece to_re2(SourceString str) {
  return to_re2(str.Str());
}

// Returns the substring matching a valid AttributeName, advancing `source_str`
// to the following character. If no such substring exists, returns
// `absl::nullopt` and leaves `source_str` untouched. This is like matching the
// regex `^[A-Z0-9-]+`.
absl::optional<SourceString> ExtractAttributeName(SourceString* source_str) {
  auto str = *source_str;

  // Returns whether the given char is permitted in an AttributeName
  const auto is_char_valid = [](char c) -> bool {
    if (c >= 'A' && c <= 'Z') {
      return true;
    }
    if (c >= '0' && c <= '9') {
      return true;
    }
    if (c == '-') {
      return true;
    }

    return false;
  };

  // Extract the substring where `is_char_valid` succeeds
  const char* end =
      std::find_if_not(str.Str().cbegin(), str.Str().cend(), is_char_valid);
  const auto name = str.Consume(end - str.Str().cbegin());

  // At least one character must have matched
  if (name.Empty()) {
    return absl::nullopt;
  }

  *source_str = str;
  return name;
}

// Returns the substring matching a valid AttributeValue, advancing `source_str`
// to the following character. If no such substring exists, returns
// `absl::nullopt` and leaves `source_str` untouched. This like like matching
// the regex `^[a-zA-Z0-9_.-]+|"[^"\r\n]*"`.
absl::optional<SourceString> ExtractAttributeValue(SourceString* source_str) {
  // Cache string to stack so we don't modify it unless its valid
  auto str = *source_str;

  // Empty strings are not allowed
  if (str.Empty()) {
    return absl::nullopt;
  }

  // If this is a quoted attribute value, get everything between the matching
  // quotes
  if (*str.Str().begin() == '"') {
    const auto matching_quote = str.Str().find_first_of("\"\r\n", 1);

    // If match wasn't found, value isn't valid
    if (matching_quote == base::StringPiece::npos) {
      return absl::nullopt;
    }

    // If match was not '"', value isn't valid
    if (str.Str().at(matching_quote) != '"') {
      return absl::nullopt;
    }

    const auto result = str.Consume(matching_quote + 1);
    *source_str = str;
    return result;
  }

  // Otherwise, extract valid unquoted chars.
  // This returns whether a given char is permitted in an unquoted attribute
  // value.
  const auto is_char_valid = [](char c) -> bool {
    if (c >= 'a' && c <= 'z') {
      return true;
    }
    if (c >= 'A' && c <= 'Z') {
      return true;
    }
    if (c >= '0' && c <= '9') {
      return true;
    }
    if (c == '-' || c == '_' || c == '.') {
      return true;
    }

    return false;
  };

  const char* end =
      std::find_if_not(str.Str().cbegin(), str.Str().cend(), is_char_valid);
  const auto result = str.Consume(end - str.Str().cbegin());

  // At least one character must have matched
  if (result.Empty()) {
    return absl::nullopt;
  }

  *source_str = str;
  return result;
}

struct AttributeMapComparator {
  bool operator()(const AttributeMap::Item& left,
                  const AttributeMap::Item& right) {
    return left.first < right.first;
  }
  bool operator()(const AttributeMap::Item& left, SourceString right) {
    return left.first < right.Str();
  }
};

}  // namespace

ParseStatus::Or<DecimalInteger> ParseDecimalInteger(SourceString source_str) {
  static const base::NoDestructor<re2::RE2> decimal_integer_regex("\\d{1,20}");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: 0-9
  // NOTE: It may be useful to split this into a separate function which
  // extracts the range containing valid characters from a given
  // base::StringPiece. For now that's the caller's responsibility.
  if (!RE2::FullMatch(to_re2(str), *decimal_integer_regex)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  DecimalInteger result;
  if (!base::StringToUint64(str, &result)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  return result;
}

ParseStatus::Or<DecimalFloatingPoint> ParseDecimalFloatingPoint(
    SourceString source_str) {
  // Utilize signed parsing function
  auto result = ParseSignedDecimalFloatingPoint(source_str);
  if (result.has_error()) {
    return ParseStatusCode::kFailedToParseDecimalFloatingPoint;
  }

  // Decimal-floating-point values may not be negative (including -0.0)
  SignedDecimalFloatingPoint value = std::move(result).value();
  if (std::signbit(value)) {
    return ParseStatusCode::kFailedToParseDecimalFloatingPoint;
  }

  return value;
}

ParseStatus::Or<SignedDecimalFloatingPoint> ParseSignedDecimalFloatingPoint(
    SourceString source_str) {
  // Accept no decimal point, decimal point with leading digits, trailing
  // digits, or both
  static const base::NoDestructor<re2::RE2> decimal_floating_point_regex(
      "-?(\\d+|\\d+\\.|\\.\\d+|\\d+\\.\\d+)");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: - . 0-9
  // `base::StringToDouble` is not as strict as the HLS spec
  if (!re2::RE2::FullMatch(to_re2(str), *decimal_floating_point_regex)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  DecimalFloatingPoint result;
  const bool success = base::StringToDouble(str, &result);
  if (!success || !std::isfinite(result)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  return result;
}

ParseStatus::Or<base::StringPiece> ParseQuotedString(
    SourceString source_str,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  return ParseQuotedStringWithoutSubstitution(source_str)
      .MapValue([&variable_dict, &sub_buffer](auto str) {
        return variable_dict.Resolve(str, sub_buffer);
      });
}

ParseStatus::Or<SourceString> ParseQuotedStringWithoutSubstitution(
    SourceString source_str) {
  if (source_str.Size() < 2) {
    return ParseStatusCode::kFailedToParseQuotedString;
  }
  if (*source_str.Str().begin() != '"') {
    return ParseStatusCode::kFailedToParseQuotedString;
  }
  if (*source_str.Str().rbegin() != '"') {
    return ParseStatusCode::kFailedToParseQuotedString;
  }

  return source_str.Substr(1, source_str.Size() - 2);
}

AttributeListIterator::AttributeListIterator(SourceString content)
    : remaining_content_(content) {}

ParseStatus::Or<AttributeListIterator::Item> AttributeListIterator::Next() {
  // Cache `remaining_content_` to the stack so that if we error out,
  // we'll continue returning the same error.
  auto content = remaining_content_;

  // Empty string is tolerated, but caller must handle this case.
  if (content.Empty()) {
    return ParseStatusCode::kReachedEOF;
  }

  // The remainder of the function expects a string matching
  // {AttributeName}={AttributeValue}[,][...]

  // Extract attribute name
  const auto name = ExtractAttributeName(&content);
  if (!name.has_value()) {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Next character must be '='
  if (content.Consume(1).Str() != "=") {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Extract attribute value
  const auto value = ExtractAttributeValue(&content);
  if (!value.has_value()) {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Following character must either be a comma, or the end of the string
  // The wording of the spec doesn't explicitly allow or reject trailing
  // commas. Since they appear in other contexts (and they're great) I'm going
  // to support them.
  if (!content.Empty() && content.Consume(1).Str() != ",") {
    return ParseStatusCode::kMalformedAttributeList;
  }

  remaining_content_ = content;
  return Item{.name = name.value(), .value = value.value()};
}

AttributeMap::AttributeMap(base::span<Item> sorted_items)
    : items_(sorted_items) {
  // This is a DCHECK because the set of items should be fixed at compile-time
  // by the caller, so they're expected to have sorted it themselves.
  // The given keys are also expected to be unique, however that's a more
  // difficult mistake to make and the failure there would occur if the owner
  // tries to access the stored value after filling by the index of a subsequent
  // duplicate key, rather than the first.
  DCHECK(
      std::is_sorted(items_.begin(), items_.end(), AttributeMapComparator()));
}

AttributeMap::~AttributeMap() = default;
AttributeMap::AttributeMap(const AttributeMap&) = default;
AttributeMap::AttributeMap(AttributeMap&&) = default;
AttributeMap& AttributeMap::operator=(const AttributeMap&) = default;
AttributeMap& AttributeMap::operator=(AttributeMap&&) = default;

ParseStatus::Or<AttributeListIterator::Item> AttributeMap::Fill(
    AttributeListIterator* iter) {
  while (true) {
    // Cache iter to stack, in case we hit a duplicate
    const auto iter_backup = *iter;

    auto result = iter->Next();
    if (result.has_error()) {
      return result;
    }

    auto item = std::move(result).value();

    // Look up the item. std::lower_bound performs a binary search to find the
    // first item where the name comparison function fails.
    auto entry = std::lower_bound(items_.begin(), items_.end(), item.name,
                                  AttributeMapComparator());
    if (entry == items_.end()) {
      return item;
    }
    if (entry->first != item.name.Str()) {
      return item;
    }

    // There was a match, so either assign the value or return a duplicate error
    if (entry->second.has_value()) {
      // Rewind iterator
      *iter = iter_backup;
      return ParseStatusCode::kAttributeListHasDuplicateNames;
    }
    entry->second = item.value;
  }
}

ParseStatus AttributeMap::FillUntilError(AttributeListIterator* iter) {
  while (true) {
    auto result = Fill(iter);
    if (result.has_error()) {
      return std::move(result).error();
    }

    // TODO(crbug.com/1266991): It may be worth recording a UMA here, to
    // discover common unhandled attributes. Since we can't plug arbitrary
    // strings into UMA this will require some additional design work.
  }
}

ParseStatus::Or<VariableName> VariableName::Parse(SourceString source_str) {
  static const base::NoDestructor<re2::RE2> variable_name_regex(
      "[a-zA-Z0-9_-]+");

  // This source_str must match completely
  if (!re2::RE2::FullMatch(to_re2(source_str), *variable_name_regex)) {
    return ParseStatusCode::kMalformedVariableName;
  }

  return VariableName{source_str.Str()};
}

}  // namespace media::hls::types
