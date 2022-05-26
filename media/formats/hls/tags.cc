// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"

#include <cstddef>
#include <type_traits>
#include <utility>
#include "base/notreached.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/variable_dictionary.h"

namespace media::hls {

namespace {

template <typename T>
ParseStatus::Or<T> ParseEmptyTag(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(T::kName));
  if (tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  return T{};
}

template <typename T>
ParseStatus::Or<T> ParseDecimalIntegerTag(TagItem tag,
                                          types::DecimalInteger T::*field) {
  DCHECK(tag.GetName() == ToTagName(T::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  auto value = types::ParseDecimalInteger(*tag.GetContent());
  if (value.has_error()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(value).error());
  }

  T out;
  out.*field = std::move(value).value();
  return out;
}

// Attributes expected in `EXT-X-DEFINE` tag contents.
// These must remain sorted alphabetically.
enum class XDefineTagAttribute {
  kImport,
  kName,
  kValue,
  kMaxValue = kValue,
};

constexpr base::StringPiece GetAttributeName(XDefineTagAttribute attribute) {
  switch (attribute) {
    case XDefineTagAttribute::kImport:
      return "IMPORT";
    case XDefineTagAttribute::kName:
      return "NAME";
    case XDefineTagAttribute::kValue:
      return "VALUE";
  }

  NOTREACHED();
  return "";
}

// Attributes expected in `EXT-X-STREAM-INF` tag contents.
// These must remain sorted alphabetically.
enum class XStreamInfTagAttribute {
  kAverageBandwidth,
  kBandwidth,
  kCodecs,
  kFrameRate,
  kProgramId,  // Ignored for backwards compatibility
  kResolution,
  kScore,
  kMaxValue = kScore,
};

constexpr base::StringPiece GetAttributeName(XStreamInfTagAttribute attribute) {
  switch (attribute) {
    case XStreamInfTagAttribute::kAverageBandwidth:
      return "AVERAGE-BANDWIDTH";
    case XStreamInfTagAttribute::kBandwidth:
      return "BANDWIDTH";
    case XStreamInfTagAttribute::kCodecs:
      return "CODECS";
    case XStreamInfTagAttribute::kFrameRate:
      return "FRAME-RATE";
    case XStreamInfTagAttribute::kProgramId:
      return "PROGRAM-ID";
    case XStreamInfTagAttribute::kResolution:
      return "RESOLUTION";
    case XStreamInfTagAttribute::kScore:
      return "SCORE";
  }

  NOTREACHED();
  return "";
}

// Attributes expected in `EXT-X-PART-INF` tag contents.
// These must remain sorted alphabetically.
enum class XPartInfTagAttribute {
  kPartTarget,
  kMaxValue = kPartTarget,
};

constexpr base::StringPiece GetAttributeName(XPartInfTagAttribute attribute) {
  switch (attribute) {
    case XPartInfTagAttribute::kPartTarget:
      return "PART-TARGET";
  }

  NOTREACHED();
  return "";
}

template <typename T, size_t kLast>
constexpr bool IsAttributeEnumSorted(std::index_sequence<kLast>) {
  return true;
}

template <typename T, size_t kLHS, size_t kRHS, size_t... kRest>
constexpr bool IsAttributeEnumSorted(
    std::index_sequence<kLHS, kRHS, kRest...>) {
  const auto lhs = GetAttributeName(static_cast<T>(kLHS));
  const auto rhs = GetAttributeName(static_cast<T>(kRHS));
  return lhs < rhs &&
         IsAttributeEnumSorted<T>(std::index_sequence<kRHS, kRest...>{});
}

// Wraps `AttributeMap::MakeStorage` by mapping the (compile-time) sequence
// of size_t's to a sequence of the corresponding attribute enum names.
template <typename T, std::size_t... Indices>
constexpr std::array<types::AttributeMap::Item, sizeof...(Indices)>
MakeTypedAttributeMapStorage(std::index_sequence<Indices...> seq) {
  static_assert(IsAttributeEnumSorted<T>(seq),
                "Enum keys must be sorted alphabetically");
  return types::AttributeMap::MakeStorage(
      GetAttributeName(static_cast<T>(Indices))...);
}

// Helper for using AttributeMap with an enum of keys.
// The result of running `GetAttributeName` across `0..T::kMaxValue` (inclusive)
// must produced an ordered set of strings.
template <typename T>
struct TypedAttributeMap {
  static_assert(std::is_enum<T>::value, "T must be an enum");
  static_assert(std::is_same<decltype(GetAttributeName(std::declval<T>())),
                             base::StringPiece>::value,
                "GetAttributeName must be overloaded for T to return a "
                "base::StringPiece");
  static constexpr size_t kNumKeys = static_cast<size_t>(T::kMaxValue) + 1;

  TypedAttributeMap()
      : attributes_(MakeTypedAttributeMapStorage<T>(
            std::make_index_sequence<kNumKeys>())) {}

  // Wraps `AttributeMap::FillUntilError` using the built-in storage object.
  ParseStatus FillUntilError(types::AttributeListIterator* iter) {
    types::AttributeMap map(attributes_);
    return map.FillUntilError(iter);
  }

  // Returns whether the entry corresponding to the given key has a value.
  bool HasValue(T key) const {
    return attributes_[static_cast<size_t>(key)].second.has_value();
  }

  // Returns the value stored in the entry for the given key.
  SourceString GetValue(T key) const {
    return attributes_[static_cast<size_t>(key)].second.value();
  }

 private:
  std::array<types::AttributeMap::Item, kNumKeys> attributes_;
};

}  // namespace

ParseStatus::Or<M3uTag> M3uTag::Parse(TagItem tag) {
  return ParseEmptyTag<M3uTag>(tag);
}

ParseStatus::Or<XVersionTag> XVersionTag::Parse(TagItem tag) {
  auto result = ParseDecimalIntegerTag(tag, &XVersionTag::version);
  if (result.has_error()) {
    return std::move(result).error();
  }

  // Reject invalid version numbers.
  // For valid version numbers, caller will decide if the version is supported.
  auto out = std::move(result).value();
  if (out.version == 0) {
    return ParseStatusCode::kInvalidPlaylistVersion;
  }

  return out;
}

ParseStatus::Or<InfTag> InfTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(InfTag::kName));

  if (!tag.GetContent()) {
    return ParseStatusCode::kMalformedTag;
  }
  auto content = *tag.GetContent();

  // Inf tags have the form #EXTINF:<duration>,[<title>]
  // Find the comma.
  auto comma = content.Str().find_first_of(',');
  if (comma == base::StringPiece::npos) {
    return ParseStatusCode::kMalformedTag;
  }

  auto duration_str = content.Substr(0, comma);
  auto title_str = content.Substr(comma + 1);

  // Extract duration
  // TODO(crbug.com/1284763): Below version 3 this should be rounded to an
  // integer
  auto duration = types::ParseDecimalFloatingPoint(duration_str);
  if (duration.has_error()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(duration).error());
  }

  return InfTag{.duration = std::move(duration).value(), .title = title_str};
}

ParseStatus::Or<XIndependentSegmentsTag> XIndependentSegmentsTag::Parse(
    TagItem tag) {
  return ParseEmptyTag<XIndependentSegmentsTag>(tag);
}

ParseStatus::Or<XEndListTag> XEndListTag::Parse(TagItem tag) {
  return ParseEmptyTag<XEndListTag>(tag);
}

ParseStatus::Or<XIFramesOnlyTag> XIFramesOnlyTag::Parse(TagItem tag) {
  return ParseEmptyTag<XIFramesOnlyTag>(tag);
}

ParseStatus::Or<XDiscontinuityTag> XDiscontinuityTag::Parse(TagItem tag) {
  return ParseEmptyTag<XDiscontinuityTag>(tag);
}

ParseStatus::Or<XGapTag> XGapTag::Parse(TagItem tag) {
  return ParseEmptyTag<XGapTag>(tag);
}

XDefineTag XDefineTag::CreateDefinition(types::VariableName name,
                                        base::StringPiece value) {
  return XDefineTag{.name = name, .value = value};
}

XDefineTag XDefineTag::CreateImport(types::VariableName name) {
  return XDefineTag{.name = name, .value = absl::nullopt};
}

ParseStatus::Or<XDefineTag> XDefineTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XDefineTag::kName));

  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list.
  // Quoted strings in EXT-X-DEFINE tags are unique in that they aren't subject
  // to variable substitution. For that reason, we use the
  // `ParseQuotedStringWithoutSubstitution` function here.
  TypedAttributeMap<XDefineTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto result = map.FillUntilError(&iter);

  if (result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(result));
  }

  // "NAME" and "IMPORT" are mutually exclusive
  if (map.HasValue(XDefineTagAttribute::kName) &&
      map.HasValue(XDefineTagAttribute::kImport)) {
    return ParseStatusCode::kMalformedTag;
  }

  if (map.HasValue(XDefineTagAttribute::kName)) {
    auto var_name = types::ParseQuotedStringWithoutSubstitution(
                        map.GetValue(XDefineTagAttribute::kName))
                        .MapValue(types::VariableName::Parse);
    if (var_name.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(var_name).error());
    }

    // If "NAME" is defined, "VALUE" must also be defined
    if (!map.HasValue(XDefineTagAttribute::kValue)) {
      return ParseStatusCode::kMalformedTag;
    }

    auto value = types::ParseQuotedStringWithoutSubstitution(
        map.GetValue(XDefineTagAttribute::kValue));
    if (value.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag);
    }

    return XDefineTag::CreateDefinition(std::move(var_name).value(),
                                        std::move(value).value().Str());
  }

  if (map.HasValue(XDefineTagAttribute::kImport)) {
    auto var_name = types::ParseQuotedStringWithoutSubstitution(
                        map.GetValue(XDefineTagAttribute::kImport))
                        .MapValue(types::VariableName::Parse);
    if (var_name.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(var_name).error());
    }

    // "VALUE" doesn't make any sense here, but the spec doesn't explicitly
    // forbid it. It may be used in the future to provide a default value for
    // undefined imported variables, so we won't error on it.
    return XDefineTag::CreateImport(std::move(var_name).value());
  }

  // Without "NAME" or "IMPORT", the tag is malformed
  return ParseStatusCode::kMalformedTag;
}

ParseStatus::Or<XPlaylistTypeTag> XPlaylistTypeTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XPlaylistTypeTag::kName));

  // This tag requires content
  if (!tag.GetContent().has_value() || tag.GetContent()->Empty()) {
    return ParseStatusCode::kMalformedTag;
  }

  if (tag.GetContent()->Str() == "EVENT") {
    return XPlaylistTypeTag{.type = PlaylistType::kEvent};
  }
  if (tag.GetContent()->Str() == "VOD") {
    return XPlaylistTypeTag{.type = PlaylistType::kVOD};
  }

  return ParseStatusCode::kUnknownPlaylistType;
}

XStreamInfTag::XStreamInfTag() = default;

XStreamInfTag::~XStreamInfTag() = default;

XStreamInfTag::XStreamInfTag(const XStreamInfTag&) = default;

XStreamInfTag::XStreamInfTag(XStreamInfTag&&) = default;

XStreamInfTag& XStreamInfTag::operator=(const XStreamInfTag&) = default;

XStreamInfTag& XStreamInfTag::operator=(XStreamInfTag&&) = default;

ParseStatus::Or<XStreamInfTag> XStreamInfTag::Parse(
    TagItem tag,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  DCHECK(tag.GetName() == ToTagName(XStreamInfTag::kName));
  XStreamInfTag out;

  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list
  TypedAttributeMap<XStreamInfTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  // Extract the 'BANDWIDTH' attribute
  if (map.HasValue(XStreamInfTagAttribute::kBandwidth)) {
    auto bandwidth = types::ParseDecimalInteger(
        map.GetValue(XStreamInfTagAttribute::kBandwidth));
    if (bandwidth.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(bandwidth).error());
    }

    out.bandwidth = std::move(bandwidth).value();
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  // Extract the 'AVERAGE-BANDWIDTH' attribute
  if (map.HasValue(XStreamInfTagAttribute::kAverageBandwidth)) {
    auto average_bandwidth = types::ParseDecimalInteger(
        map.GetValue(XStreamInfTagAttribute::kAverageBandwidth));
    if (average_bandwidth.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(average_bandwidth).error());
    }

    out.average_bandwidth = std::move(average_bandwidth).value();
  }

  // Extract the 'SCORE' attribute
  if (map.HasValue(XStreamInfTagAttribute::kScore)) {
    auto score = types::ParseDecimalFloatingPoint(
        map.GetValue(XStreamInfTagAttribute::kScore));
    if (score.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(score).error());
    }

    out.score = std::move(score).value();
  }

  // Extract the 'CODECS' attribute
  if (map.HasValue(XStreamInfTagAttribute::kCodecs)) {
    auto codecs =
        types::ParseQuotedString(map.GetValue(XStreamInfTagAttribute::kCodecs),
                                 variable_dict, sub_buffer);
    if (codecs.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(codecs).error());
    }
    out.codecs = std::string{std::move(codecs).value()};
  }

  // Extract the 'RESOLUTION' attribute
  if (map.HasValue(XStreamInfTagAttribute::kResolution)) {
    auto resolution = types::DecimalResolution::Parse(
        map.GetValue(XStreamInfTagAttribute::kResolution));
    if (resolution.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(resolution).error());
    }
    out.resolution = std::move(resolution).value();
  }

  // Extract the 'FRAME-RATE' attribute
  if (map.HasValue(XStreamInfTagAttribute::kFrameRate)) {
    auto frame_rate = types::ParseDecimalFloatingPoint(
        map.GetValue(XStreamInfTagAttribute::kFrameRate));
    if (frame_rate.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(frame_rate).error());
    }
    out.frame_rate = std::move(frame_rate).value();
  }

  return out;
}

ParseStatus::Or<XTargetDurationTag> XTargetDurationTag::Parse(TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XTargetDurationTag::duration);
}

ParseStatus::Or<XPartInfTag> XPartInfTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XPartInfTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list
  TypedAttributeMap<XPartInfTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  XPartInfTag out;

  // Extract the 'PART-TARGET' attribute
  if (map.HasValue(XPartInfTagAttribute::kPartTarget)) {
    auto target_duration = types::ParseDecimalFloatingPoint(
        map.GetValue(XPartInfTagAttribute::kPartTarget));
    if (target_duration.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(target_duration).error());
    }
    out.target_duration = std::move(target_duration).value();
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  return out;
}

ParseStatus::Or<XMediaSequenceTag> XMediaSequenceTag::Parse(TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XMediaSequenceTag::number);
}

ParseStatus::Or<XDiscontinuitySequenceTag> XDiscontinuitySequenceTag::Parse(
    TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XDiscontinuitySequenceTag::number);
}

ParseStatus::Or<XByteRangeTag> XByteRangeTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XByteRangeTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  auto range = types::ByteRangeExpression::Parse(*tag.GetContent());
  if (range.has_error()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(range).error());
  }

  return XByteRangeTag{.range = std::move(range).value()};
}

ParseStatus::Or<XBitrateTag> XBitrateTag::Parse(TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XBitrateTag::bitrate);
}

}  // namespace media::hls
