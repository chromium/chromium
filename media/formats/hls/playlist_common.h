// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_
#define MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_

#include "media/formats/hls/items.h"
#include "media/formats/hls/tag_name.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace media::hls {

// State common to parsing both multivariant and media playlists.
struct CommonParserState {
  absl::optional<XVersionTag> version_tag;
  absl::optional<XIndependentSegmentsTag> independent_segments_tag;

  // Returns the version specified by `version_tag`, or the default version if
  // the playlist did not contain a version tag.
  types::DecimalInteger GetVersion() const;
};

// Validates that the first line of the given SourceLineIterator contains a
// valid #EXTM3U tag.
ParseStatus::Or<M3uTag> CheckM3uTag(SourceLineIterator* src_iter);

// Handles an unknown tag.
void HandleUnknownTag(TagItem);

// Handles parsing for tags that may appear in multivariant or media playlists.
absl::optional<ParseStatus> ParseCommonTag(TagItem, CommonParserState* state);

// Attempts to parse a tag from the given item, ensuring it has not been
// already appeared in the playlist.
template <typename T>
absl::optional<ParseStatus> ParseUniqueTag(TagItem tag,
                                           absl::optional<T>& out) {
  DCHECK(tag.name == ToTagName(T::kName));

  // Ensure this tag has not already appeared.
  if (out.has_value()) {
    return ParseStatusCode::kPlaylistHasDuplicateTags;
  }

  auto tag_result = T::Parse(tag);
  if (tag_result.has_error()) {
    return std::move(tag_result).error();
  }
  out = std::move(tag_result).value();

  return absl::nullopt;
}

ParseStatus::Or<GURL> ParseUri(UriItem item, const GURL& playlist_uri);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_
