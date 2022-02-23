// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAGS_H_
#define MEDIA_FORMATS_HLS_TAGS_H_

#include "media/base/media_export.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {
namespace hls {

// Represents the contents of the #EXTM3U tag
struct M3uTag {
  static constexpr TagKind kKind = TagKind::kM3u;
  static MEDIA_EXPORT ParseStatus::Or<M3uTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-VERSION tag
struct XVersionTag {
  static constexpr TagKind kKind = TagKind::kXVersion;
  static MEDIA_EXPORT ParseStatus::Or<XVersionTag> Parse(TagItem);

  types::DecimalInteger version;
};

// Represents the contents of the #EXTINF tag
struct InfTag {
  static constexpr TagKind kKind = TagKind::kInf;
  static MEDIA_EXPORT ParseStatus::Or<InfTag> Parse(TagItem);

  // Target duration of the media segment, in seconds.
  types::DecimalFloatingPoint duration;

  // Human-readable title of the media segment.
  SourceString title;
};

// Represents the contents of the #EXT-X-INDEPENDENT-SEGMENTS tag
struct XIndependentSegmentsTag {
  static constexpr TagKind kKind = TagKind::kXIndependentSegments;
  static MEDIA_EXPORT ParseStatus::Or<XIndependentSegmentsTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-ENDLIST tag
struct XEndListTag {
  static constexpr TagKind kKind = TagKind::kXEndList;
  static MEDIA_EXPORT ParseStatus::Or<XEndListTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-I-FRAMES-ONLY tag
struct XIFramesOnlyTag {
  static constexpr TagKind kKind = TagKind::kXIFramesOnly;
  static MEDIA_EXPORT ParseStatus::Or<XIFramesOnlyTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-DISCONTINUITY tag
struct XDiscontinuityTag {
  static constexpr TagKind kKind = TagKind::kXDiscontinuity;
  static MEDIA_EXPORT ParseStatus::Or<XDiscontinuityTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-GAP tag
struct XGapTag {
  static constexpr TagKind kKind = TagKind::kXGap;
  static MEDIA_EXPORT ParseStatus::Or<XGapTag> Parse(TagItem);
};

// Represents the contents of the #EXT-X-DEFINE tag
struct XDefineTag {
  static constexpr TagKind kKind = TagKind::kXDefine;
  static MEDIA_EXPORT ParseStatus::Or<XDefineTag> Parse(TagItem);

  // Constructs an XDefineTag representing a variable definition.
  static MEDIA_EXPORT XDefineTag CreateDefinition(types::VariableName name,
                                                  base::StringPiece value);

  // Constructs an XDefineTag representing an imported variable definition.
  static MEDIA_EXPORT XDefineTag CreateImport(types::VariableName name);

  // The name of the variable being defined.
  types::VariableName name;

  // The value of the variable. If this is `nullopt`, then the value
  // is being IMPORT-ed and must be defined in the parent playlist.
  absl::optional<base::StringPiece> value;
};

}  // namespace hls
}  // namespace media

#endif  // MEDIA_FORMATS_HLS_TAGS_H_
