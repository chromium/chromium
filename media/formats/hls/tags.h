// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_TAGS_H_
#define MEDIA_FORMATS_HLS_TAGS_H_

#include "media/base/media_export.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/types.h"

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

}  // namespace hls
}  // namespace media

#endif  // MEDIA_FORMATS_HLS_TAGS_H_
