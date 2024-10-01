// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/types.h"

#include <cmath>
#include <functional>
#include <limits>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/source_string.h"
#include "third_party/re2/src/re2/re2.h"

namespace media::hls::types {

namespace parsing {

// static
ParseStatus::Or<base::TimeDelta> TimeDelta::Parse(ResolvedSourceString str) {
  return ParseDecimalFloatingPoint(str).MapValue(
      [](DecimalFloatingPoint t) -> ParseStatus::Or<base::TimeDelta> {
        auto duration = base::Seconds(t);
        if (duration.is_max()) {
          return ParseStatusCode::kValueOverflowsTimeDelta;
        }
        return duration;
      });
}

// static
ParseStatus::Or<ByteRangeExpression> ByteRangeExpression::Parse(
    ResolvedSourceString source_str) {
  // If this ByteRange has an offset, it will be separated from the length by
  // '@'.
  const auto at_index = source_str.Str().find_first_of('@');
  const auto length_str = source_str.Consume(at_index);
  auto length = ParseDecimalInteger(length_str);
  if (!length.has_value()) {
    return ParseStatus(ParseStatusCode::kFailedToParseByteRange)
        .AddCause(std::move(length).error());
  }

  // If the offset was present, try to parse it
  std::optional<types::DecimalInteger> offset;
  if (at_index != std::string_view::npos) {
    source_str.Consume(1);
    auto offset_result = ParseDecimalInteger(source_str);
    if (!offset_result.has_value()) {
      return ParseStatus(ParseStatusCode::kFailedToParseByteRange)
          .AddCause(std::move(offset_result).error());
    }

    offset = std::move(offset_result).value();
  }

  return ByteRangeExpression{.length = std::move(length).value(),
                             .offset = offset};
}

// static
ParseStatus::Or<ResolvedSourceString> RawStr::Parse(ResolvedSourceString str) {
  return str;
}

// static
ParseStatus::Or<bool> YesOrNo::Parse(ResolvedSourceString str) {
  return str.Str() == "YES";
}

}  // namespace parsing

namespace {
bool IsOneOf(char c, std::string_view set) {
  return base::Contains(set, c);
}

// Returns the substring matching a valid AttributeName, advancing `source_str`
// to the following character. If no such substring exists, returns
// `std::nullopt` and leaves `source_str` untouched. This is like matching the
// regex `^[A-Z0-9-]+`.
std::optional<SourceString> ExtractAttributeName(SourceString* source_str) {
  SourceString str = *source_str;

  // Returns whether the given char is permitted in an AttributeName
  const auto is_char_valid = [](char c) -> bool {
    return base::IsAsciiUpper(c) || base::IsAsciiDigit(c) || c == '-';
  };

  // Extract the substring where `is_char_valid` succeeds
  auto end = base::ranges::find_if_not(str.Str(), is_char_valid);
  const auto name = str.Consume(end - str.Str().cbegin());

  // At least one character must have matched
  if (name.Empty()) {
    return std::nullopt;
  }

  *source_str = str;
  return name;
}

// Returns the substring matching a valid AttributeValue, advancing `source_str`
// to the following character. If no such substring exists, returns
// `std::nullopt` and leaves `source_str` untouched.
// Attribute values may either be quoted or unquoted.
// Quoted attribute values begin and end with a double-quote ("), and may
// contain internal whitespace and commas. Unquoted attribute values must not
// begin with a double-quote, but may contain any character excluding whitespace
// and commas.
std::optional<SourceString> ExtractAttributeValue(SourceString* source_str) {
  // Cache string to stack so we don't modify it unless its valid
  auto str = *source_str;

  // Empty strings are not allowed
  if (str.Empty()) {
    return std::nullopt;
  }

  // If this is a quoted attribute value, get everything between the matching
  // quotes
  if (*str.Str().begin() == '"') {
    const auto matching_quote = str.Str().find('"', 1);

    // If match wasn't found, value isn't valid
    if (matching_quote == std::string_view::npos) {
      return std::nullopt;
    }

    const auto result = str.Consume(matching_quote + 1);
    *source_str = str;
    return result;
  }

  // Otherwise, extract characters up to the next comma or whitespace. This must
  // not be empty.
  const auto end = str.Str().find_first_of(", \t");
  const auto result = str.Consume(end);
  if (result.Empty()) {
    return std::nullopt;
  }

  *source_str = str;
  return result;
}

}  // namespace

ParseStatus::Or<DecimalInteger> ParseDecimalInteger(
    ResolvedSourceString source_str) {
  static const base::NoDestructor<re2::RE2> decimal_integer_regex("\\d{1,20}");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: 0-9
  // NOTE: It may be useful to split this into a separate function which
  // extracts the range containing valid characters from a given
  // std::string_view. For now that's the caller's responsibility.
  if (!RE2::FullMatch(str, *decimal_integer_regex)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  DecimalInteger result;
  if (!base::StringToUint64(str, &result)) {
    return ParseStatusCode::kFailedToParseDecimalInteger;
  }

  return result;
}

ParseStatus::Or<DecimalFloatingPoint> ParseDecimalFloatingPoint(
    ResolvedSourceString source_str) {
  // Utilize signed parsing function
  auto result = ParseSignedDecimalFloatingPoint(source_str);
  if (!result.has_value()) {
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
    ResolvedSourceString source_str) {
  // Accept no decimal point, decimal point with leading digits, trailing
  // digits, or both
  static const base::NoDestructor<re2::RE2> decimal_floating_point_regex(
      "-?(\\d+|\\d+\\.|\\.\\d+|\\d+\\.\\d+)");

  const auto str = source_str.Str();

  // Check that the set of characters is allowed: - . 0-9
  // `base::StringToDouble` is not as strict as the HLS spec
  if (!re2::RE2::FullMatch(str, *decimal_floating_point_regex)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  DecimalFloatingPoint result;
  const bool success = base::StringToDouble(str, &result);
  if (!success || !std::isfinite(result)) {
    return ParseStatusCode::kFailedToParseSignedDecimalFloatingPoint;
  }

  return result;
}

// static
ParseStatus::Or<DecimalResolution> DecimalResolution::Parse(
    ResolvedSourceString source_str) {
  // decimal-resolution values are in the format: DecimalInteger 'x'
  // DecimalInteger
  const auto x_index = source_str.Str().find_first_of('x');
  if (x_index == std::string_view::npos) {
    return ParseStatusCode::kFailedToParseDecimalResolution;
  }

  // Extract width and height strings
  const auto width_str = source_str.Consume(x_index);
  source_str.Consume(1);
  const auto height_str = source_str;

  auto width = ParseDecimalInteger(width_str);
  auto height = ParseDecimalInteger(height_str);
  for (auto* x : {&width, &height}) {
    if (!x->has_value()) {
      return ParseStatus(ParseStatusCode::kFailedToParseDecimalResolution)
          .AddCause(std::move(*x).error());
    }
  }

  return DecimalResolution{.width = std::move(width).value(),
                           .height = std::move(height).value()};
}


// static
std::optional<ByteRange> ByteRange::Validate(DecimalInteger length,
                                             DecimalInteger offset) {
  if (length == 0) {
    return std::nullopt;
  }

  // Ensure that `length+offset` won't overflow `DecimalInteger`
  if (std::numeric_limits<DecimalInteger>::max() - offset < length) {
    return std::nullopt;
  }

  return ByteRange(length, offset);
}

ParseStatus::Or<ResolvedSourceString> ParseQuotedString(
    SourceString source_str,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer,
    bool allow_empty) {
  return ParseQuotedStringWithoutSubstitution(source_str, allow_empty)
      .MapValue([&variable_dict, &sub_buffer](auto str) {
        return variable_dict.Resolve(str, sub_buffer);
      })
      .MapValue(
          [allow_empty](auto str) -> ParseStatus::Or<ResolvedSourceString> {
            if (!allow_empty && str.Empty()) {
              return ParseStatusCode::kFailedToParseQuotedString;
            } else {
              return str;
            }
          });
}

ParseStatus::Or<SourceString> ParseQuotedStringWithoutSubstitution(
    SourceString source_str,
    bool allow_empty) {
  if (source_str.Size() < 2) {
    return ParseStatusCode::kFailedToParseQuotedString;
  }
  if (*source_str.Str().begin() != '"') {
    return ParseStatusCode::kFailedToParseQuotedString;
  }
  if (*source_str.Str().rbegin() != '"') {
    return ParseStatusCode::kFailedToParseQuotedString;
  }

  auto str = source_str.Substr(1, source_str.Size() - 2);
  if (!allow_empty && str.Empty()) {
    return ParseStatusCode::kFailedToParseQuotedString;
  }

  return str;
}

AttributeListIterator::AttributeListIterator(SourceString content)
    : remaining_content_(content) {}

ParseStatus::Or<AttributeListIterator::Item> AttributeListIterator::Next() {
  // Cache `remaining_content_` to the stack so that if we error out,
  // we'll continue returning the same error.
  auto content = remaining_content_;

  // Whitespace is allowed preceding the attribute name
  content.TrimStart();

  // Empty string is tolerated, but caller must handle this case.
  if (content.Empty()) {
    return ParseStatusCode::kReachedEOF;
  }

  // The remainder of the function expects a string matching
  // {AttributeName}[ ]=[ ]{AttributeValue}[ ][,[...]]

  // Extract attribute name
  const auto name = ExtractAttributeName(&content);
  if (!name.has_value()) {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Whitespace is allowed following the attribute name
  content.TrimStart();

  // Next character must be '='
  if (content.Consume(1).Str() != "=") {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Whitespace is allowed preceding the attribute value
  content.TrimStart();

  // Extract attribute value
  const auto value = ExtractAttributeValue(&content);
  if (!value.has_value()) {
    return ParseStatusCode::kMalformedAttributeList;
  }

  // Whitespace is allowed following attribute value
  content.TrimStart();

  // Following character must either be a comma, or the end of the string
  // Trailing commas are allowed (not explicitly by the spec, but supported by
  // Safari).
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
      base::ranges::is_sorted(items_, std::less(), &AttributeMap::Item::first));
}

ParseStatus::Or<AttributeListIterator::Item> AttributeMap::Fill(
    AttributeListIterator* iter) {
  while (true) {
    // Cache iter to stack, in case we hit a duplicate
    const auto iter_backup = *iter;

    auto result = iter->Next();
    if (!result.has_value()) {
      return result;
    }

    auto item = std::move(result).value();

    // Look up the item. `base::ranges::lower_bound` performs a binary search to
    // find the first entry where the name does not compare less than the given
    // value.
    auto entry = base::ranges::lower_bound(items_, item.name.Str(), std::less(),
                                           &AttributeMap::Item::first);
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
    if (!result.has_value()) {
      return std::move(result).error();
    }

    // TODO(crbug.com/40057824): It may be worth recording a UMA here, to
    // discover common unhandled attributes. Since we can't plug arbitrary
    // strings into UMA this will require some additional design work.
  }
}

AttributeMap::~AttributeMap() = default;

// static
ParseStatus::Or<VariableName> VariableName::Parse(SourceString source_str) {
  static const base::NoDestructor<re2::RE2> variable_name_regex(
      "[a-zA-Z0-9_-]+");

  // This source_str must match completely
  if (!re2::RE2::FullMatch(source_str.Str(), *variable_name_regex)) {
    return ParseStatusCode::kMalformedVariableName;
  }

  return VariableName(source_str.Str());
}

// static
ParseStatus::Or<StableId> StableId::Parse(ResolvedSourceString str) {
  const auto is_char_valid = [](char c) -> bool {
    return base::IsAsciiAlphaNumeric(c) || IsOneOf(c, "+/=.-_");
  };

  if (str.Empty() || !base::ranges::all_of(str.Str(), is_char_valid)) {
    return ParseStatusCode::kFailedToParseStableId;
  }

  return StableId(std::string{str.Str()});
}

// static
ParseStatus::Or<InstreamId> InstreamId::Parse(ResolvedSourceString str) {
  constexpr std::string_view kCcStr = "CC";
  constexpr std::string_view kServiceStr = "SERVICE";

  // Parse the type (one of 'CC' or 'SERVICE')
  Type type;
  uint8_t max;
  if (base::StartsWith(str.Str(), kCcStr)) {
    type = Type::kCc;
    max = 4;
    str.Consume(kCcStr.size());
  } else if (base::StartsWith(str.Str(), kServiceStr)) {
    type = Type::kService;
    max = 63;
    str.Consume(kServiceStr.size());
  } else {
    return ParseStatusCode::kFailedToParseInstreamId;
  }

  // Parse the number, max allowed value depends on the type
  auto number_result = ParseDecimalInteger(str);
  if (!number_result.has_value()) {
    return ParseStatusCode::kFailedToParseInstreamId;
  }
  auto number = std::move(number_result).value();
  if (number < 1 || number > max) {
    return ParseStatusCode::kFailedToParseInstreamId;
  }

  return InstreamId(type, static_cast<uint8_t>(number));
}

AudioChannels::AudioChannels(DecimalInteger max_channels,
                             std::vector<std::string> audio_coding_identifiers)
    : max_channels_(max_channels),
      audio_coding_identifiers_(std::move(audio_coding_identifiers)) {}

AudioChannels::AudioChannels(const AudioChannels&) = default;

AudioChannels::AudioChannels(AudioChannels&&) = default;

AudioChannels& AudioChannels::operator=(const AudioChannels&) = default;

AudioChannels& AudioChannels::operator=(AudioChannels&&) = default;

AudioChannels::~AudioChannels() = default;

// static
ParseStatus::Or<AudioChannels> AudioChannels::Parse(ResolvedSourceString str) {
  // First parameter is a decimal-integer indicating the number of channels
  const auto max_channels_str = str.ConsumeDelimiter('/');
  auto max_channels_result = ParseDecimalInteger(max_channels_str);
  if (!max_channels_result.has_value()) {
    return ParseStatus(ParseStatusCode::kFailedToParseAudioChannels)
        .AddCause(std::move(max_channels_result).error());
  }
  const auto max_channels = std::move(max_channels_result).value();

  // Second parameter (optional) is a comma-seperated list of audio coding
  // identifiers.
  auto audio_coding_identifiers_str = str.ConsumeDelimiter('/');
  std::vector<std::string> audio_coding_identifiers;
  while (!audio_coding_identifiers_str.Empty()) {
    const auto identifier = audio_coding_identifiers_str.ConsumeDelimiter(',');

    constexpr auto is_valid_coding_identifier_char = [](char c) -> bool {
      return base::IsAsciiUpper(c) || base::IsAsciiDigit(c) || c == '-';
    };

    // Each string must be non-empty and consist only of the allowed characters
    if (identifier.Empty() ||
        !base::ranges::all_of(identifier.Str(),
                              is_valid_coding_identifier_char)) {
      return ParseStatusCode::kFailedToParseAudioChannels;
    }

    audio_coding_identifiers.emplace_back(identifier.Str());
  }

  // Ignore any remaining parameters for forward-compatibility
  return AudioChannels(max_channels, std::move(audio_coding_identifiers));
}

DecimalInteger DecimalResolution::Szudzik() const {
  if (width > (1 << 16) || height > (1 << 16)) {
    // resolutions greater than 32768 are not allowed!
    return 0;
  }
  // See http://szudzik.com/ElegantPairing.pdf for the math
  return (width >= height) ? (width * width + width + height)
                           : (height * height + width);
}

}  // namespace media::hls::types
