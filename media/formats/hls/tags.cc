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

namespace media::hls {

namespace {

template <typename T>
ParseStatus::Or<T> ParseEmptyTag(TagItem tag) {
  DCHECK(tag.name == ToTagName(T::kName));
  if (!tag.content.Str().empty()) {
    return ParseStatusCode::kMalformedTag;
  }

  return T{};
}

// Quoted strings in EXT-X-DEFINE tags are unique in that they aren't subject to
// variable substitution. To simplify things, we define a special quoted-string
// extraction function here.
ParseStatus::Or<SourceString> ParseXDefineTagQuotedString(
    SourceString source_str) {
  if (source_str.Str().size() < 2) {
    return ParseStatusCode::kMalformedTag;
  }

  if (*source_str.Str().begin() != '"') {
    return ParseStatusCode::kMalformedTag;
  }

  if (*source_str.Str().rbegin() != '"') {
    return ParseStatusCode::kMalformedTag;
  }

  return source_str.Substr(1, source_str.Str().size() - 2);
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
  DCHECK(tag.name == ToTagName(XVersionTag::kName));

  auto value_result = types::ParseDecimalInteger(tag.content);
  if (value_result.has_error()) {
    return ParseStatus(ParseStatusCode::kMalformedTag)
        .AddCause(std::move(value_result).error());
  }

  // Reject invalid version numbers.
  // For valid version numbers, caller will decide if the version is supported.
  auto value = std::move(value_result).value();
  if (value == 0) {
    return ParseStatusCode::kInvalidPlaylistVersion;
  }

  return XVersionTag{.version = value};
}

ParseStatus::Or<InfTag> InfTag::Parse(TagItem tag) {
  DCHECK(tag.name == ToTagName(InfTag::kName));

  // Inf tags have the form #EXTINF:<duration>,[<title>]
  // Find the comma.
  auto comma = tag.content.Str().find_first_of(',');
  if (comma == base::StringPiece::npos) {
    return ParseStatusCode::kMalformedTag;
  }

  auto duration_str = tag.content.Substr(0, comma);
  auto title_str = tag.content.Substr(comma + 1);

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
  // Parse the attribute-list
  TypedAttributeMap<XDefineTagAttribute> map;
  types::AttributeListIterator iter(tag.content);
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
    auto var_name =
        ParseXDefineTagQuotedString(map.GetValue(XDefineTagAttribute::kName))
            .MapValue(types::ParseVariableName);
    if (var_name.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag)
          .AddCause(std::move(var_name).error());
    }

    // If "NAME" is defined, "VALUE" must also be defined
    if (!map.HasValue(XDefineTagAttribute::kValue)) {
      return ParseStatusCode::kMalformedTag;
    }

    auto value =
        ParseXDefineTagQuotedString(map.GetValue(XDefineTagAttribute::kValue));
    if (value.has_error()) {
      return ParseStatus(ParseStatusCode::kMalformedTag);
    }

    return XDefineTag::CreateDefinition(std::move(var_name).value(),
                                        std::move(value).value().Str());
  }

  if (map.HasValue(XDefineTagAttribute::kImport)) {
    auto var_name =
        ParseXDefineTagQuotedString(map.GetValue(XDefineTagAttribute::kImport))
            .MapValue(types::ParseVariableName);
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

}  // namespace media::hls
