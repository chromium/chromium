// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_
#define MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_

#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/tag_name.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "url/gurl.h"

namespace media::hls {

// State common to parsing both multivariant and media playlists.
struct CommonParserState {
  std::optional<XVersionTag> version_tag;
  std::optional<XIndependentSegmentsTag> independent_segments_tag;

  // The dictionary for variables defined in the current playlist.
  VariableDictionary variable_dict;

  // The dictionary of variables defined in the parent playlist. This may remain
  // null if there is no parent playlist (in the case of a multivariant
  // playlist, or a media playlist without other variants).
  raw_ptr<const VariableDictionary> parent_variable_dict = nullptr;

  // Checks that the versions given by `expected_version` and `version_tag`
  // match. If `version_tag` is `std::nullopt`, the version given is implicitly
  // `Playlist::kDefaultVersion`.
  bool CheckVersion(types::DecimalInteger expected_version) const;
};

// Validates that the first line of the given SourceLineIterator contains a
// valid #EXTM3U tag.
ParseStatus::Or<M3uTag> CheckM3uTag(SourceLineIterator* src_iter);

// Handles an unknown tag.
void HandleUnknownTag(TagItem);

// Handles parsing for tags that may appear in multivariant or media playlists.
std::optional<ParseStatus> ParseCommonTag(TagItem, CommonParserState* state);

// Attempts to parse a tag from the given item, ensuring it has not been
// already appeared in the playlist.
template <typename T, typename... Args>
std::optional<ParseStatus> ParseUniqueTag(TagItem tag,
                                          std::optional<T>& out,
                                          Args&&... args) {
  DCHECK(tag.GetName() == ToTagName(T::kName));

  // Ensure this tag has not already appeared.
  if (out.has_value()) {
    return ParseStatusCode::kPlaylistHasDuplicateTags;
  }

  auto tag_result = T::Parse(tag, std::forward<Args>(args)...);
  if (!tag_result.has_value())
    return std::move(tag_result).error();
  out = std::move(tag_result).value();

  return std::nullopt;
}

ParseStatus::Or<GURL> ParseUri(UriItem item,
                               const GURL& playlist_uri,
                               const CommonParserState& state,
                               VariableDictionary::SubstitutionBuffer& buffer);

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_PLAYLIST_COMMON_H_
