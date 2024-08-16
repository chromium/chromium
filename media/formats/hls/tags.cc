// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"

#include <cstddef>
#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "media/base/mime_util.h"
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

  auto value =
      types::ParseDecimalInteger(tag.GetContent()->SkipVariableSubstitution());
  if (!value.has_value()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(value).error());
  }

  T out;
  out.*field = std::move(value).value();
  return out;
}

template <typename T>
ParseStatus::Or<T> ParseISO8601DateTimeTag(TagItem tag, base::Time T::*field) {
  CHECK(tag.GetName() == ToTagName(T::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }
  const auto content = tag.GetContent()->SkipVariableSubstitution().Str();
  std::string content_nullterm = std::string(content);
  T out;
  base::Time time;
  if (base::Time::FromString(content_nullterm.c_str(), &time)) {
    out.*field = time;
  } else {
    return ParseStatusCode::kMalformedTag;
  }
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

constexpr std::string_view GetAttributeName(XDefineTagAttribute attribute) {
  switch (attribute) {
    case XDefineTagAttribute::kImport:
      return "IMPORT";
    case XDefineTagAttribute::kName:
      return "NAME";
    case XDefineTagAttribute::kValue:
      return "VALUE";
  }

  NOTREACHED();
}

// Attributes expected in `EXT-X-MEDIA` tag contents.
// These must remain sorted alphabetically.
enum class XMediaTagAttribute {
  kAssocLanguage,
  kAutoselect,
  kChannels,
  kCharacteristics,
  kDefault,
  kForced,
  kGroupId,
  kInstreamId,
  kLanguage,
  kName,
  kStableRenditionId,
  kType,
  kUri,
  kMaxValue = kUri,
};

constexpr std::string_view GetAttributeName(XMediaTagAttribute attribute) {
  switch (attribute) {
    case XMediaTagAttribute::kAssocLanguage:
      return "ASSOC-LANGUAGE";
    case XMediaTagAttribute::kAutoselect:
      return "AUTOSELECT";
    case XMediaTagAttribute::kChannels:
      return "CHANNELS";
    case XMediaTagAttribute::kCharacteristics:
      return "CHARACTERISTICS";
    case XMediaTagAttribute::kDefault:
      return "DEFAULT";
    case XMediaTagAttribute::kForced:
      return "FORCED";
    case XMediaTagAttribute::kGroupId:
      return "GROUP-ID";
    case XMediaTagAttribute::kInstreamId:
      return "INSTREAM-ID";
    case XMediaTagAttribute::kLanguage:
      return "LANGUAGE";
    case XMediaTagAttribute::kName:
      return "NAME";
    case XMediaTagAttribute::kStableRenditionId:
      return "STABLE-RENDITION-ID";
    case XMediaTagAttribute::kType:
      return "TYPE";
    case XMediaTagAttribute::kUri:
      return "URI";
  }

  NOTREACHED();
}

// Attributes expected in `EXT-X-STREAM-INF` tag contents.
// These must remain sorted alphabetically.
enum class XStreamInfTagAttribute {
  kAudio,
  kAverageBandwidth,
  kBandwidth,
  kCodecs,
  kFrameRate,
  kProgramId,  // Ignored for backwards compatibility
  kResolution,
  kScore,
  kVideo,
  kMaxValue = kVideo,
};

constexpr std::string_view GetAttributeName(XStreamInfTagAttribute attribute) {
  switch (attribute) {
    case XStreamInfTagAttribute::kAudio:
      return "AUDIO";
    case XStreamInfTagAttribute::kVideo:
      return "VIDEO";
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
}

// Attributes expected in `EXT-X-SKIP` tag contents.
// These must remain sorted alphabetically.
enum class XSkipTagAttribute {
  kRecentlyRemovedDateranges,
  kSkippedSegments,
  kMaxValue = kSkippedSegments,
};

constexpr std::string_view GetAttributeName(XSkipTagAttribute attribute) {
  switch (attribute) {
    case XSkipTagAttribute::kRecentlyRemovedDateranges:
      return "RECENTLY-REMOVED-DATERANGES";
    case XSkipTagAttribute::kSkippedSegments:
      return "SKIPPED-SEGMENTS";
  }
  NOTREACHED();
}

enum class XRenditionReportTagAttribute {
  kLastMSN,
  kLastPart,
  kUri,
  kMaxValue = kUri,
};

constexpr std::string_view GetAttributeName(XRenditionReportTagAttribute attr) {
  switch (attr) {
    case XRenditionReportTagAttribute::kUri:
      return "URI";
    case XRenditionReportTagAttribute::kLastMSN:
      return "LAST-MSN";
    case XRenditionReportTagAttribute::kLastPart:
      return "LAST-PART";
  }
  NOTREACHED();
}

// Attributes expected in `EXT-X-MAP` tag contents.
// These must remain sorted alphabetically.
enum class XMapTagAttribute {
  kByteRange,
  kUri,
  kMaxValue = kUri,
};

constexpr std::string_view GetAttributeName(XMapTagAttribute attribute) {
  switch (attribute) {
    case XMapTagAttribute::kByteRange:
      return "BYTERANGE";
    case XMapTagAttribute::kUri:
      return "URI";
  }

  NOTREACHED();
}

// Attributes expected in `EXT-X-PART` tag contents.
// These must remain sorted alphabetically.
enum class XPartTagAttribute {
  kByteRange,
  kDuration,
  kGap,
  kIndependent,
  kUri,
  kMaxValue = kUri,
};

constexpr std::string_view GetAttributeName(XPartTagAttribute attribute) {
  switch (attribute) {
    case XPartTagAttribute::kByteRange:
      return "BYTERANGE";
    case XPartTagAttribute::kDuration:
      return "DURATION";
    case XPartTagAttribute::kGap:
      return "GAP";
    case XPartTagAttribute::kIndependent:
      return "INDEPENDENT";
    case XPartTagAttribute::kUri:
      return "URI";
  }
}

// Attributes expected in `EXT-X-PART-INF` tag contents.
// These must remain sorted alphabetically.
enum class XPartInfTagAttribute {
  kPartTarget,
  kMaxValue = kPartTarget,
};

constexpr std::string_view GetAttributeName(XPartInfTagAttribute attribute) {
  switch (attribute) {
    case XPartInfTagAttribute::kPartTarget:
      return "PART-TARGET";
  }

  NOTREACHED();
}

// Attributes expected in `EXT-X-SERVER-CONTROL` tag contents.
// These must remain sorted alphabetically.
enum class XServerControlTagAttribute {
  kCanBlockReload,
  kCanSkipDateRanges,
  kCanSkipUntil,
  kHoldBack,
  kPartHoldBack,
  kMaxValue = kPartHoldBack,
};

constexpr std::string_view GetAttributeName(
    XServerControlTagAttribute attribute) {
  switch (attribute) {
    case XServerControlTagAttribute::kCanBlockReload:
      return "CAN-BLOCK-RELOAD";
    case XServerControlTagAttribute::kCanSkipDateRanges:
      return "CAN-SKIP-DATERANGES";
    case XServerControlTagAttribute::kCanSkipUntil:
      return "CAN-SKIP-UNTIL";
    case XServerControlTagAttribute::kHoldBack:
      return "HOLD-BACK";
    case XServerControlTagAttribute::kPartHoldBack:
      return "PART-HOLD-BACK";
  }

  NOTREACHED();
}

enum class XKeyTagAttribute {
  kIv,
  kKeyFormat,
  kKeyFormatVersions,
  kMethod,
  kUri,
  kMaxValue = kUri,
};

constexpr std::string_view GetAttributeName(XKeyTagAttribute attribute) {
  switch (attribute) {
    case XKeyTagAttribute::kIv:
      return "IV";
    case XKeyTagAttribute::kKeyFormat:
      return "KEYFORMAT";
    case XKeyTagAttribute::kKeyFormatVersions:
      return "KEYFORMATVERSIONS";
    case XKeyTagAttribute::kMethod:
      return "METHOD";
    case XKeyTagAttribute::kUri:
      return "URI";
  }
  NOTREACHED();
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
                             std::string_view>::value,
                "GetAttributeName must be overloaded for T to return a "
                "std::string_view");
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

  size_t Size() const {
    return std::count_if(attributes_.begin(), attributes_.end(),
                         [](const types::AttributeMap::Item& item) {
                           return item.second.has_value();
                         });
  }

 private:
  std::array<types::AttributeMap::Item, kNumKeys> attributes_;
};

#define RETURN_IF_ERROR(var_not_expr)                   \
  do {                                                  \
    if (!var_not_expr.has_value()) {                    \
      return std::move(var_not_expr).error().AddHere(); \
    }                                                   \
  } while (0)

template <template <typename...> typename SpecifiedContainer, typename Type>
struct is_specialization_of : std::false_type {};

template <template <typename...> typename SpecifiedContainer, typename... Types>
struct is_specialization_of<SpecifiedContainer, SpecifiedContainer<Types...>>
    : std::true_type {};

// ParseField exists to help with the pattern where we need to parse some field
// from a dictionary of tag attributes where some of those fields are optional.
// It is designed to provide support for either optional or required fields,
// depending on the return-type specialization:
// When Result=std::optional<X>, ParseField will return a nullopt rather than
// an error if the `field_name` field can't be found in `map`.
// When Result=T (where T is not std::optional<X>), ParseField will fail with
// a ParseStatus if `field_name` is not found in map.
// A function pointer must also be provided which does the parsing from
// SourceString => Result, provided that the `field_name` key is present.
template <typename Result,
          typename AttrEnum,
          typename ParseFn,
          typename... ParseFnArgs>
ParseStatus::Or<Result> ParseField(AttrEnum field_name,
                                   TypedAttributeMap<AttrEnum> map,
                                   ParseFn parser,
                                   ParseFnArgs&&... args) {
  if (map.HasValue(field_name)) {
    auto maybe =
        parser(map.GetValue(field_name), std::forward<ParseFnArgs>(args)...);
    if (!maybe.has_value()) {
      ParseStatus result = ParseStatusCode::kMalformedTag;
      return std::move(result).AddCause(std::move(maybe).error());
    }
    return Result(std::move(maybe).value());
  }
  if constexpr (is_specialization_of<std::optional, Result>::value) {
    return Result(std::nullopt);
  }
  return ParseStatusCode::kMalformedTag;
}

}  // namespace

// static
ParseStatus::Or<M3uTag> M3uTag::Parse(TagItem tag) {
  return ParseEmptyTag<M3uTag>(tag);
}

// static
XDefineTag XDefineTag::CreateDefinition(types::VariableName name,
                                        std::string_view value) {
  return XDefineTag{.name = name, .value = value};
}

// static
XDefineTag XDefineTag::CreateImport(types::VariableName name) {
  return XDefineTag{.name = name, .value = std::nullopt};
}

// static
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
    if (!var_name.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(var_name).error());
    }

    // If "NAME" is defined, "VALUE" must also be defined
    if (!map.HasValue(XDefineTagAttribute::kValue)) {
      return ParseStatusCode::kMalformedTag;
    }

    auto value = types::ParseQuotedStringWithoutSubstitution(
        map.GetValue(XDefineTagAttribute::kValue), /*allow_empty*/ true);
    if (!value.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag);
    }

    return XDefineTag::CreateDefinition(std::move(var_name).value(),
                                        std::move(value).value().Str());
  }

  if (map.HasValue(XDefineTagAttribute::kImport)) {
    auto var_name = types::ParseQuotedStringWithoutSubstitution(
                        map.GetValue(XDefineTagAttribute::kImport))
                        .MapValue(types::VariableName::Parse);
    if (!var_name.has_value()) {
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

// static
ParseStatus::Or<XIndependentSegmentsTag> XIndependentSegmentsTag::Parse(
    TagItem tag) {
  return ParseEmptyTag<XIndependentSegmentsTag>(tag);
}

// static
ParseStatus::Or<XVersionTag> XVersionTag::Parse(TagItem tag) {
  auto result = ParseDecimalIntegerTag(tag, &XVersionTag::version);
  if (!result.has_value()) {
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

struct XMediaTag::CtorArgs {
  decltype(XMediaTag::type) type;
  decltype(XMediaTag::uri) uri;
  decltype(XMediaTag::instream_id) instream_id;
  decltype(XMediaTag::group_id) group_id;
  decltype(XMediaTag::language) language;
  decltype(XMediaTag::associated_language) associated_language;
  decltype(XMediaTag::name) name;
  decltype(XMediaTag::stable_rendition_id) stable_rendition_id;
  decltype(XMediaTag::is_default) is_default;
  decltype(XMediaTag::autoselect) autoselect;
  decltype(XMediaTag::forced) forced;
  decltype(XMediaTag::characteristics) characteristics;
  decltype(XMediaTag::channels) channels;
};

XMediaTag::XMediaTag(CtorArgs args)
    : type(std::move(args.type)),
      uri(std::move(args.uri)),
      instream_id(std::move(args.instream_id)),
      group_id(std::move(args.group_id)),
      language(std::move(args.language)),
      associated_language(std::move(args.associated_language)),
      name(std::move(args.name)),
      stable_rendition_id(std::move(args.stable_rendition_id)),
      is_default(std::move(args.is_default)),
      autoselect(std::move(args.autoselect)),
      forced(std::move(args.forced)),
      characteristics(std::move(args.characteristics)),
      channels(std::move(args.channels)) {}

XMediaTag::~XMediaTag() = default;

XMediaTag::XMediaTag(const XMediaTag&) = default;

XMediaTag::XMediaTag(XMediaTag&&) = default;

XMediaTag& XMediaTag::operator=(const XMediaTag&) = default;

XMediaTag& XMediaTag::operator=(XMediaTag&&) = default;

// static
ParseStatus::Or<XMediaTag> XMediaTag::Parse(
    TagItem tag,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  DCHECK(tag.GetName() == ToTagName(XMediaTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list
  TypedAttributeMap<XMediaTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  // Parse the 'TYPE' attribute
  MediaType type;
  if (map.HasValue(XMediaTagAttribute::kType)) {
    auto str = map.GetValue(XMediaTagAttribute::kType);
    if (str.Str() == "AUDIO") {
      type = MediaType::kAudio;
    } else if (str.Str() == "VIDEO") {
      type = MediaType::kVideo;
    } else if (str.Str() == "SUBTITLES") {
      type = MediaType::kSubtitles;
    } else if (str.Str() == "CLOSED-CAPTIONS") {
      type = MediaType::kClosedCaptions;
    } else {
      return ParseStatusCode::kMalformedTag;
    }
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the 'URI' attribute
  std::optional<ResolvedSourceString> uri;
  if (map.HasValue(XMediaTagAttribute::kUri)) {
    // This attribute MUST NOT be defined for closed-captions renditions
    if (type == MediaType::kClosedCaptions) {
      return ParseStatusCode::kMalformedTag;
    }

    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kUri), variable_dict, sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    uri = std::move(result).value();
  } else {
    // URI MUST be defined for subtitle renditions
    if (type == MediaType::kSubtitles) {
      return ParseStatusCode::kMalformedTag;
    }
  }

  // Parse the 'GROUP-ID' attribute
  std::optional<ResolvedSourceString> group_id;
  if (map.HasValue(XMediaTagAttribute::kGroupId)) {
    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kGroupId), variable_dict, sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    group_id = std::move(result).value();
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the 'LANGUAGE' attribute
  std::optional<ResolvedSourceString> language;
  if (map.HasValue(XMediaTagAttribute::kLanguage)) {
    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kLanguage), variable_dict, sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    language = std::move(result).value();
  }

  // Parse the 'ASSOC-LANGUAGE' attribute
  std::optional<ResolvedSourceString> assoc_language;
  if (map.HasValue(XMediaTagAttribute::kAssocLanguage)) {
    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kAssocLanguage), variable_dict,
        sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    assoc_language = std::move(result).value();
  }

  // Parse the 'NAME' attribute
  std::optional<ResolvedSourceString> name;
  if (map.HasValue(XMediaTagAttribute::kName)) {
    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kName), variable_dict, sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    name = std::move(result).value();
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the 'STABLE-RENDITION-ID' attribute
  std::optional<types::StableId> stable_rendition_id;
  if (map.HasValue(XMediaTagAttribute::kStableRenditionId)) {
    auto result = types::ParseQuotedString(
                      map.GetValue(XMediaTagAttribute::kStableRenditionId),
                      variable_dict, sub_buffer)
                      .MapValue(types::StableId::Parse);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }
    stable_rendition_id = std::move(result).value();
  }

  // Parse the 'DEFAULT' attribute
  bool is_default = false;
  if (map.HasValue(XMediaTagAttribute::kDefault)) {
    if (map.GetValue(XMediaTagAttribute::kDefault).Str() == "YES") {
      is_default = true;
    }
  }

  // Parse the 'AUTOSELECT' attribute
  bool autoselect = is_default;
  if (map.HasValue(XMediaTagAttribute::kAutoselect)) {
    if (map.GetValue(XMediaTagAttribute::kAutoselect).Str() == "YES") {
      autoselect = true;
    } else if (is_default) {
      // If the 'DEFAULT' attribute is 'YES', then the value of this attribute
      // must also be 'YES', if present.
      return ParseStatusCode::kMalformedTag;
    }
  }

  // Parse the 'FORCED' attribute
  bool forced = false;
  if (map.HasValue(XMediaTagAttribute::kForced)) {
    // The FORCED attribute MUST NOT be present unless TYPE=SUBTITLES
    if (type != MediaType::kSubtitles) {
      return ParseStatusCode::kMalformedTag;
    }

    if (map.GetValue(XMediaTagAttribute::kForced).Str() == "YES") {
      forced = true;
    }
  }

  // Parse the 'INSTREAM-ID' attribute
  std::optional<types::InstreamId> instream_id;
  if (map.HasValue(XMediaTagAttribute::kInstreamId)) {
    // The INSTREAM-ID attribute MUST NOT be present unless TYPE=CLOSED-CAPTIONS
    if (type != MediaType::kClosedCaptions) {
      return ParseStatusCode::kMalformedTag;
    }

    auto result =
        types::ParseQuotedString(map.GetValue(XMediaTagAttribute::kInstreamId),
                                 variable_dict, sub_buffer)
            .MapValue(types::InstreamId::Parse);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    instream_id = std::move(result).value();
  }
  // This attribute is REQUIRED if TYPE=CLOSED-CAPTIONS
  else if (type == MediaType::kClosedCaptions) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the 'CHARACTERISTICS' attribute
  std::vector<std::string> characteristics;
  if (map.HasValue(XMediaTagAttribute::kCharacteristics)) {
    auto result = types::ParseQuotedString(
        map.GetValue(XMediaTagAttribute::kCharacteristics), variable_dict,
        sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }
    auto value = std::move(result).value();

    while (!value.Empty()) {
      const auto mct = value.ConsumeDelimiter(',');
      if (mct.Empty()) {
        return ParseStatusCode::kMalformedTag;
      }

      characteristics.emplace_back(mct.Str());
    }
  }

  // Parse the 'CHANNELS' attribute
  std::optional<types::AudioChannels> channels;
  if (map.HasValue(XMediaTagAttribute::kChannels)) {
    // Currently only supported type with channel information is `kAudio`.
    if (type == MediaType::kAudio) {
      auto result =
          types::ParseQuotedString(map.GetValue(XMediaTagAttribute::kChannels),
                                   variable_dict, sub_buffer)
              .MapValue(types::AudioChannels::Parse);
      if (!result.has_value()) {
        return ParseStatus(ParseStatusCode::kMalformedTag)
            .AddCause(std::move(result).error());
      }

      channels = std::move(result).value();
    }
  }

  return XMediaTag(XMediaTag::CtorArgs{
      .type = type,
      .uri = uri,
      .instream_id = instream_id,
      .group_id = group_id.value(),
      .language = language,
      .associated_language = assoc_language,
      .name = name.value(),
      .stable_rendition_id = std::move(stable_rendition_id),
      .is_default = is_default,
      .autoselect = autoselect,
      .forced = forced,
      .characteristics = std::move(characteristics),
      .channels = std::move(channels),
  });
}

XStreamInfTag::XStreamInfTag() = default;

XStreamInfTag::~XStreamInfTag() = default;

XStreamInfTag::XStreamInfTag(const XStreamInfTag&) = default;

XStreamInfTag::XStreamInfTag(XStreamInfTag&&) = default;

XStreamInfTag& XStreamInfTag::operator=(const XStreamInfTag&) = default;

XStreamInfTag& XStreamInfTag::operator=(XStreamInfTag&&) = default;

// static
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
        map.GetValue(XStreamInfTagAttribute::kBandwidth)
            .SkipVariableSubstitution());
    if (!bandwidth.has_value()) {
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
        map.GetValue(XStreamInfTagAttribute::kAverageBandwidth)
            .SkipVariableSubstitution());
    if (!average_bandwidth.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(average_bandwidth).error());
    }

    out.average_bandwidth = std::move(average_bandwidth).value();
  }

  // Extract the 'SCORE' attribute
  if (map.HasValue(XStreamInfTagAttribute::kScore)) {
    auto score = types::ParseDecimalFloatingPoint(
        map.GetValue(XStreamInfTagAttribute::kScore)
            .SkipVariableSubstitution());
    if (!score.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(score).error());
    }

    out.score = std::move(score).value();
  }

  // Extract the 'CODECS' attribute
  if (map.HasValue(XStreamInfTagAttribute::kCodecs)) {
    auto codecs_string =
        types::ParseQuotedString(map.GetValue(XStreamInfTagAttribute::kCodecs),
                                 variable_dict, sub_buffer);
    if (!codecs_string.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(codecs_string).error());
    }

    // Split the list of codecs
    std::vector<std::string> codecs;
    SplitCodecs(std::move(codecs_string).value().Str(), &codecs);
    out.codecs = std::move(codecs);
  }

  // Extract the 'RESOLUTION' attribute
  if (map.HasValue(XStreamInfTagAttribute::kResolution)) {
    auto resolution = types::DecimalResolution::Parse(
        map.GetValue(XStreamInfTagAttribute::kResolution)
            .SkipVariableSubstitution());
    if (!resolution.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(resolution).error());
    }
    out.resolution = std::move(resolution).value();
  }

  // Extract the 'FRAME-RATE' attribute
  if (map.HasValue(XStreamInfTagAttribute::kFrameRate)) {
    auto frame_rate = types::ParseDecimalFloatingPoint(
        map.GetValue(XStreamInfTagAttribute::kFrameRate)
            .SkipVariableSubstitution());
    if (!frame_rate.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(frame_rate).error());
    }
    out.frame_rate = std::move(frame_rate).value();
  }

  // Extract the 'AUDIO' attribute
  if (map.HasValue(XStreamInfTagAttribute::kAudio)) {
    auto audio =
        types::ParseQuotedString(map.GetValue(XStreamInfTagAttribute::kAudio),
                                 variable_dict, sub_buffer);
    if (!audio.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(audio).error());
    }
    out.audio = std::move(audio).value();
  }

  // Extract the 'VIDEO' attribute
  if (map.HasValue(XStreamInfTagAttribute::kVideo)) {
    auto video =
        types::ParseQuotedString(map.GetValue(XStreamInfTagAttribute::kVideo),
                                 variable_dict, sub_buffer);
    if (!video.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(video).error());
    }
    out.video = std::move(video).value();
  }

  return out;
}

// static
ParseStatus::Or<InfTag> InfTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(InfTag::kName));

  if (!tag.GetContent()) {
    return ParseStatusCode::kMalformedTag;
  }
  auto content = *tag.GetContent();

  // Inf tags have the form #EXTINF:<duration>,[<title>]
  // Find the comma.
  auto comma = content.Str().find_first_of(',');
  SourceString duration_str = content;
  SourceString title_str = content;
  if (comma == std::string_view::npos) {
    // While the HLS spec does require commas at the end of inf tags, it's
    // incredibly common for sites to elide the comma if there is no title
    // attribute present. In this case, we should assert that there is at least
    // a trailing newline, and then strip it to generate a nameless tag.
    title_str = content.Substr(0, 0);
    if (*content.Str().end() != '\n') {
      duration_str = content;
    } else {
      duration_str = content.Substr(0, content.Str().length() - 1);
    }
  } else {
    duration_str = content.Substr(0, comma);
    title_str = content.Substr(comma + 1);
  }

  // Extract duration
  // TODO(crbug.com/40210233): Below version 3 this should be rounded to an
  // integer
  auto duration_result =
      types::ParseDecimalFloatingPoint(duration_str.SkipVariableSubstitution());
  if (!duration_result.has_value()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(duration_result).error());
  }
  const auto duration = base::Seconds(std::move(duration_result).value());

  if (duration.is_max()) {
    return ParseStatusCode::kValueOverflowsTimeDelta;
  }

  return InfTag{.duration = duration, .title = title_str};
}

// static
ParseStatus::Or<XBitrateTag> XBitrateTag::Parse(TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XBitrateTag::bitrate);
}

// static
ParseStatus::Or<XByteRangeTag> XByteRangeTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XByteRangeTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  auto range = types::parsing::ByteRangeExpression::Parse(
      tag.GetContent()->SkipVariableSubstitution());
  if (!range.has_value()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(range).error());
  }

  return XByteRangeTag{.range = std::move(range).value()};
}

// static
ParseStatus::Or<XDiscontinuityTag> XDiscontinuityTag::Parse(TagItem tag) {
  return ParseEmptyTag<XDiscontinuityTag>(tag);
}

// static
ParseStatus::Or<XDiscontinuitySequenceTag> XDiscontinuitySequenceTag::Parse(
    TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XDiscontinuitySequenceTag::number);
}

// static
ParseStatus::Or<XEndListTag> XEndListTag::Parse(TagItem tag) {
  return ParseEmptyTag<XEndListTag>(tag);
}

// static
ParseStatus::Or<XGapTag> XGapTag::Parse(TagItem tag) {
  return ParseEmptyTag<XGapTag>(tag);
}

// static
ParseStatus::Or<XIFramesOnlyTag> XIFramesOnlyTag::Parse(TagItem tag) {
  return ParseEmptyTag<XIFramesOnlyTag>(tag);
}

// static
ParseStatus::Or<XMapTag> XMapTag::Parse(
    TagItem tag,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  DCHECK(tag.GetName() == ToTagName(XMapTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list
  TypedAttributeMap<XMapTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  std::optional<ResolvedSourceString> uri;
  if (map.HasValue(XMapTagAttribute::kUri)) {
    auto result = types::ParseQuotedString(map.GetValue(XMapTagAttribute::kUri),
                                           variable_dict, sub_buffer);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    uri = std::move(result).value();
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  std::optional<types::parsing::ByteRangeExpression> byte_range;
  if (map.HasValue(XMapTagAttribute::kByteRange)) {
    auto result =
        types::ParseQuotedString(map.GetValue(XMapTagAttribute::kByteRange),
                                 variable_dict, sub_buffer)
            .MapValue(types::parsing::ByteRangeExpression::Parse);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    byte_range = std::move(result).value();
  }

  return XMapTag{.uri = uri.value(), .byte_range = byte_range};
}

// static
ParseStatus::Or<XMediaSequenceTag> XMediaSequenceTag::Parse(TagItem tag) {
  return ParseDecimalIntegerTag(tag, &XMediaSequenceTag::number);
}

// static
ParseStatus::Or<XPartTag> XPartTag::Parse(
    TagItem tag,
    const VariableDictionary& vars,
    VariableDictionary::SubstitutionBuffer& subs) {
  DCHECK(tag.GetName() == ToTagName(XPartTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  TypedAttributeMap<XPartTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  auto uri = ParseField<ResolvedSourceString>(
      XPartTagAttribute::kUri, map,
      &types::parsing::Quoted<types::parsing::RawStr>::ParseWithSubstitution,
      vars, subs);
  RETURN_IF_ERROR(uri);

  auto duration = ParseField<base::TimeDelta>(
      XPartTagAttribute::kDuration, map,
      &types::parsing::TimeDelta::ParseWithoutSubstitution);
  RETURN_IF_ERROR(duration);

  auto byte_range =
      ParseField<std::optional<types::parsing::ByteRangeExpression>>(
          XPartTagAttribute::kByteRange, map,
          &types::parsing::Quoted<
              types::parsing::ByteRangeExpression>::ParseWithSubstitution,
          vars, subs);
  RETURN_IF_ERROR(byte_range);

  auto independent = ParseField<std::optional<bool>>(
      XPartTagAttribute::kIndependent, map,
      &types::parsing::YesOrNo::ParseWithoutSubstitution);
  RETURN_IF_ERROR(independent);

  auto gap = ParseField<std::optional<bool>>(
      XPartTagAttribute::kGap, map,
      &types::parsing::YesOrNo::ParseWithoutSubstitution);
  RETURN_IF_ERROR(gap);

  return XPartTag{.uri = std::move(uri).value(),
                  .duration = std::move(duration).value(),
                  .byte_range = std::move(byte_range).value(),
                  .independent = (*independent).value_or(false),
                  .gap = (*gap).value_or(false)};
}

// static
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

  // Extract the 'PART-TARGET' attribute
  base::TimeDelta part_target;
  if (map.HasValue(XPartInfTagAttribute::kPartTarget)) {
    auto result = types::ParseDecimalFloatingPoint(
        map.GetValue(XPartInfTagAttribute::kPartTarget)
            .SkipVariableSubstitution());

    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    part_target = base::Seconds(std::move(result).value());

    if (part_target.is_max()) {
      return ParseStatusCode::kValueOverflowsTimeDelta;
    }
  } else {
    return ParseStatusCode::kMalformedTag;
  }

  return XPartInfTag{.target_duration = part_target};
}

// static
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

// static
ParseStatus::Or<XServerControlTag> XServerControlTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XServerControlTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  // Parse the attribute-list
  TypedAttributeMap<XServerControlTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  // Extract the 'CAN-SKIP-UNTIL' attribute
  std::optional<base::TimeDelta> can_skip_until;
  if (map.HasValue(XServerControlTagAttribute::kCanSkipUntil)) {
    auto result = types::ParseDecimalFloatingPoint(
        map.GetValue(XServerControlTagAttribute::kCanSkipUntil)
            .SkipVariableSubstitution());

    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    can_skip_until = base::Seconds(std::move(result).value());

    if (can_skip_until->is_max()) {
      return ParseStatusCode::kValueOverflowsTimeDelta;
    }
  }

  // Extract the 'CAN-SKIP-DATERANGES' attribute
  bool can_skip_dateranges = false;
  if (map.HasValue(XServerControlTagAttribute::kCanSkipDateRanges)) {
    if (map.GetValue(XServerControlTagAttribute::kCanSkipDateRanges).Str() ==
        "YES") {
      // The existence of this attribute requires the 'CAN-SKIP-UNTIL'
      // attribute.
      if (!can_skip_until.has_value()) {
        return ParseStatusCode::kMalformedTag;
      }

      can_skip_dateranges = true;
    }
  }

  // Extract the 'HOLD-BACK' attribute
  std::optional<base::TimeDelta> hold_back;
  if (map.HasValue(XServerControlTagAttribute::kHoldBack)) {
    auto result = types::ParseDecimalFloatingPoint(
        map.GetValue(XServerControlTagAttribute::kHoldBack)
            .SkipVariableSubstitution());

    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    hold_back = base::Seconds(std::move(result).value());

    if (hold_back->is_max()) {
      return ParseStatusCode::kValueOverflowsTimeDelta;
    }
  }

  // Extract the 'PART-HOLD-BACK' attribute
  std::optional<base::TimeDelta> part_hold_back;
  if (map.HasValue(XServerControlTagAttribute::kPartHoldBack)) {
    auto result = types::ParseDecimalFloatingPoint(
        map.GetValue(XServerControlTagAttribute::kPartHoldBack)
            .SkipVariableSubstitution());

    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(result).error());
    }

    part_hold_back = base::Seconds(std::move(result).value());

    if (part_hold_back->is_max()) {
      return ParseStatusCode::kValueOverflowsTimeDelta;
    }
  }

  // Extract the 'CAN-BLOCK-RELOAD' attribute
  bool can_block_reload = false;
  if (map.HasValue(XServerControlTagAttribute::kCanBlockReload)) {
    if (map.GetValue(XServerControlTagAttribute::kCanBlockReload).Str() ==
        "YES") {
      can_block_reload = true;
    }
  }

  return XServerControlTag{
      .skip_boundary = can_skip_until,
      .can_skip_dateranges = can_skip_dateranges,
      .hold_back = hold_back,
      .part_hold_back = part_hold_back,
      .can_block_reload = can_block_reload,
  };
}

// static
ParseStatus::Or<XTargetDurationTag> XTargetDurationTag::Parse(TagItem tag) {
  DCHECK(tag.GetName() == ToTagName(XTargetDurationTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  auto duration_result = types::ParseDecimalInteger(
      tag.GetContent().value().SkipVariableSubstitution());
  if (!duration_result.has_value()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(duration_result).error());
  }

  auto duration = base::Seconds(std::move(duration_result).value());
  if (duration.is_max()) {
    return ParseStatusCode::kValueOverflowsTimeDelta;
  }

  return XTargetDurationTag{.duration = duration};
}

XSkipTag::XSkipTag() = default;
XSkipTag::~XSkipTag() = default;
XSkipTag::XSkipTag(const XSkipTag&) = default;
XSkipTag::XSkipTag(XSkipTag&&) = default;

ParseStatus::Or<XSkipTag> XSkipTag::Parse(
    TagItem tag,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  DCHECK(tag.GetName() == ToTagName(XSkipTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  XSkipTag out;
  TypedAttributeMap<XSkipTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  if (!map.HasValue(XSkipTagAttribute::kSkippedSegments)) {
    return ParseStatusCode::kMalformedTag;
  }

  auto skip_result = types::ParseDecimalInteger(
      map.GetValue(XSkipTagAttribute::kSkippedSegments)
          .SkipVariableSubstitution());

  if (!skip_result.has_value()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(skip_result).error());
  }

  out.skipped_segments = std::move(skip_result).value();

  if (map.HasValue(XSkipTagAttribute::kRecentlyRemovedDateranges)) {
    // TODO(bug/314833987): Should this list have substitution?
    auto removed_result = types::ParseQuotedString(
        map.GetValue(XSkipTagAttribute::kRecentlyRemovedDateranges),
        variable_dict, sub_buffer, /*allow_empty=*/true);
    if (!removed_result.has_value()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(removed_result).error());
    }

    auto tab_joined_daterange_ids = std::move(removed_result).value();
    std::vector<std::string> removed_dateranges = {};

    while (!tab_joined_daterange_ids.Empty()) {
      const auto daterange_id = tab_joined_daterange_ids.ConsumeDelimiter('\t');
      if (daterange_id.Empty()) {
        return ParseStatusCode::kMalformedTag;
      }
      // TODO(bug/314833987): What type should this be parsed into?
      removed_dateranges.emplace_back(daterange_id.Str());
    }
    out.recently_removed_dateranges = std::move(removed_dateranges);
  }

  return out;
}

ParseStatus::Or<XRenditionReportTag> XRenditionReportTag::Parse(
    TagItem tag,
    const VariableDictionary& variable_dict,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  CHECK(tag.GetName() == ToTagName(XRenditionReportTag::kName));
  if (!tag.GetContent().has_value()) {
    return ParseStatusCode::kMalformedTag;
  }
  XRenditionReportTag out;
  TypedAttributeMap<XRenditionReportTagAttribute> map;
  types::AttributeListIterator iter(*tag.GetContent());
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return std::move(map_result).AddHere();
  }

  if (map.HasValue(XRenditionReportTagAttribute::kUri)) {
    auto uri_result = types::ParseQuotedString(
        map.GetValue(XRenditionReportTagAttribute::kUri), variable_dict,
        sub_buffer);
    if (!uri_result.has_value()) {
      return std::move(uri_result).error().AddHere();
    }
    out.uri = std::move(uri_result).value();
  }

  if (map.HasValue(XRenditionReportTagAttribute::kLastMSN)) {
    auto msn_result = types::ParseDecimalInteger(
        map.GetValue(XRenditionReportTagAttribute::kLastMSN)
            .SkipVariableSubstitution());
    if (!msn_result.has_value()) {
      return std::move(msn_result).error().AddHere();
    }
    out.last_msn = std::move(msn_result).value();
  }

  if (map.HasValue(XRenditionReportTagAttribute::kLastPart)) {
    auto part_result = types::ParseDecimalInteger(
        map.GetValue(XRenditionReportTagAttribute::kLastPart)
            .SkipVariableSubstitution());
    if (!part_result.has_value()) {
      return std::move(part_result).error().AddHere();
    }
    out.last_part = std::move(part_result).value();
  }

  return out;
}

ParseStatus::Or<XProgramDateTimeTag> XProgramDateTimeTag::Parse(TagItem tag) {
  return ParseISO8601DateTimeTag(tag, &XProgramDateTimeTag::time);
}

template <typename Attrs>
ParseStatus::Or<TypedAttributeMap<Attrs>> RequireNonEmptyMap(
    std::optional<SourceString> content) {
  if (!content.has_value()) {
    return ParseStatusCode::kMalformedTag;
  }

  TypedAttributeMap<Attrs> map;
  types::AttributeListIterator iter(*content);
  auto map_result = map.FillUntilError(&iter);

  if (map_result.code() != ParseStatusCode::kReachedEOF) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(map_result));
  }

  return map;
}

namespace {

constexpr char const* kMethodNone = "NONE";
constexpr char const* kMethodAES128 = "AES-128";
constexpr char const* kMethodAES256 = "AES-256";
constexpr char const* kMethodSampleAES = "SAMPLE-AES";
constexpr char const* kMethodSampleAESCTR = "SAMPLE-AES-CTR";
constexpr char const* kMethodSampleAESCENC = "SAMPLE-AES-CENC";
constexpr char const* kMethodISO230017 = "ISO-23001-7";

constexpr char const* kFmtClearkey = "org.w3.clearkey";
constexpr char const* kFmtWidevine =
    "urn:uuid:edef8ba9-79d6-4ace-a3c8-27dcd51d21ed";
constexpr char const* kFmtIdentity = "identity";

ParseStatus::Or<XKeyTagMethod> RecognizeMethod(SourceString content) {
  if (content.Str() == kMethodNone) {
    return XKeyTagMethod::kNone;
  }
  if (content.Str() == kMethodAES128) {
    return XKeyTagMethod::kAES128;
  } else if (content.Str() == kMethodAES256) {
    return XKeyTagMethod::kAES256;
  } else if (content.Str() == kMethodSampleAES) {
    return XKeyTagMethod::kSampleAES;
  } else if (content.Str() == kMethodSampleAESCTR) {
    return XKeyTagMethod::kSampleAESCTR;
  } else if (content.Str() == kMethodSampleAESCENC) {
    return XKeyTagMethod::kSampleAESCENC;
  } else if (content.Str() == kMethodISO230017) {
    return XKeyTagMethod::kISO230017;
  } else {
    return ParseStatusCode::kUnsupportedEncryptionMethod;
  }
}

ParseStatus::Or<XKeyTagKeyFormat> RecognizeFormat(
    std::optional<ResolvedSourceString> content) {
  if (!content.has_value()) {
    return XKeyTagKeyFormat::kIdentity;
  } else if (content->Str() == kFmtClearkey) {
    return XKeyTagKeyFormat::kClearKey;
  } else if (content->Str() == kFmtWidevine) {
    return XKeyTagKeyFormat::kWidevine;
  } else if (content->Str() == kFmtIdentity) {
    return XKeyTagKeyFormat::kIdentity;
  } else {
    return XKeyTagKeyFormat::kUnsupported;
  }
}

template <typename T>
ParseStatus::Or<T> ValidateKeyTag(
    XKeyTagMethod method,
    ResolvedSourceString uri,
    std::optional<XKeyTag::IVHex::Container> iv,
    XKeyTagKeyFormat keyformat,
    std::optional<ResolvedSourceString> keyformat_versions) {
  // Check for incompatible fields. Some methods _must not_ have an IV
  // present, while some formats only have a specific set of allowable
  // methods.
  if (iv.has_value()) {
    switch (method) {
      case XKeyTagMethod::kSampleAESCTR:
      case XKeyTagMethod::kSampleAESCENC:
      case XKeyTagMethod::kISO230017:
        return ParseStatusCode::kMalformedTag;
      default:
        break;
    }
  }
  switch (keyformat) {
    case XKeyTagKeyFormat::kIdentity:
    case XKeyTagKeyFormat::kUnsupported:
      break;  // Identity and others always are ok.
    case XKeyTagKeyFormat::kClearKey:
    case XKeyTagKeyFormat::kWidevine:
      switch (method) {
        case XKeyTagMethod::kSampleAES:
        case XKeyTagMethod::kSampleAESCTR:
        case XKeyTagMethod::kSampleAESCENC:
        case XKeyTagMethod::kISO230017:
          break;  // Acceptable methods for widevine and clearkey
        case XKeyTagMethod::kNone:
        case XKeyTagMethod::kAES128:
        case XKeyTagMethod::kAES256:
          return ParseStatusCode::kMalformedTag;
      }
      break;
  }

  return T{.method = method,
           .uri = std::move(uri),
           .iv = std::move(iv),
           .keyformat = keyformat,
           .keyformat_versions = std::move(keyformat_versions)};
}

template <typename T>
ParseStatus::Or<T> ParseKeyTag(TagItem tag,
                               const VariableDictionary& vars,
                               VariableDictionary::SubstitutionBuffer& subs) {
  return RequireNonEmptyMap<XKeyTagAttribute>(tag.GetContent())
      .MapValue([&](auto map) -> ParseStatus::Or<T> {
        auto enc_method = ParseField<XKeyTagMethod>(XKeyTagAttribute::kMethod,
                                                    map, &RecognizeMethod);
        RETURN_IF_ERROR(enc_method);

        if constexpr (T::kAllowEmptyMethod) {
          if (*enc_method == XKeyTagMethod::kNone) {
            if (map.Size() != 1) {
              return ParseStatusCode::kMalformedTag;
            }
            return T{.method = *enc_method};
          }
        }

        auto enc_uri = ParseField<ResolvedSourceString>(
            XKeyTagAttribute::kUri, map,
            &types::parsing::Quoted<
                types::parsing::RawStr>::ParseWithSubstitution,
            vars, subs);
        RETURN_IF_ERROR(enc_uri);

        auto enc_iv = ParseField<std::optional<XKeyTag::IVHex::Container>>(
            XKeyTagAttribute::kIv, map,
            &XKeyTag::IVHex::ParseWithoutSubstitution);
        RETURN_IF_ERROR(enc_iv);

        auto enc_keyformat =
            ParseField<std::optional<ResolvedSourceString>>(
                XKeyTagAttribute::kKeyFormat, map,
                &types::parsing::Quoted<
                    types::parsing::RawStr>::ParseWithoutSubstitution)
                .MapValue(&RecognizeFormat);
        RETURN_IF_ERROR(enc_keyformat);

        auto enc_keyformat_versions =
            ParseField<std::optional<ResolvedSourceString>>(
                XKeyTagAttribute::kKeyFormatVersions, map,
                &types::parsing::RawStr::ParseWithoutSubstitution);
        RETURN_IF_ERROR(enc_keyformat_versions);

        return ValidateKeyTag<T>(*enc_method, *enc_uri, *enc_iv, *enc_keyformat,
                                 *enc_keyformat_versions);
      });
}

}  // namespace

ParseStatus::Or<XKeyTag> XKeyTag::Parse(
    TagItem tag,
    const VariableDictionary& vars,
    VariableDictionary::SubstitutionBuffer& subs) {
  DCHECK(tag.GetName() == ToTagName(XKeyTag::kName));
  return ParseKeyTag<XKeyTag>(tag, vars, subs);
}

ParseStatus::Or<XSessionKeyTag> XSessionKeyTag::Parse(
    TagItem tag,
    const VariableDictionary& vars,
    VariableDictionary::SubstitutionBuffer& subs) {
  DCHECK(tag.GetName() == ToTagName(XSessionKeyTag::kName));
  return ParseKeyTag<XSessionKeyTag>(tag, vars, subs);
}

#undef RETURN_IF_ERROR

}  // namespace media::hls
