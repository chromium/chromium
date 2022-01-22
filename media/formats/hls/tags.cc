// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/tags.h"

#include <cstddef>
#include "media/formats/hls/parse_status.h"

namespace media {
namespace hls {

namespace {

template <typename T>
ParseStatus::Or<T> ParseEmptyTag(TagItem tag) {
  DCHECK(tag.kind == T::kKind);
  if (!tag.content.Str().empty()) {
    return ParseStatusCode::kMalformedTag;
  }

  return T{};
}

}  // namespace

ParseStatus::Or<M3uTag> M3uTag::Parse(TagItem tag) {
  return ParseEmptyTag<M3uTag>(tag);
}

ParseStatus::Or<XVersionTag> XVersionTag::Parse(TagItem tag) {
  DCHECK(tag.kind == TagKind::kXVersion);

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
  DCHECK(tag.kind == TagKind::kInf);

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

}  // namespace hls
}  // namespace media
