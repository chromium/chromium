// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/playlist_common.h"
#include "base/notreached.h"

namespace media::hls {

types::DecimalInteger CommonParserState::GetVersion() const {
  if (version_tag.has_value()) {
    return version_tag.value().version;
  } else {
    return 1;
  }
}

ParseStatus::Or<M3uTag> CheckM3uTag(SourceLineIterator* src_iter) {
  auto item_result = GetNextLineItem(src_iter);
  if (item_result.has_error()) {
    return ParseStatus(ParseStatusCode::kPlaylistMissingM3uTag)
        .AddCause(std::move(item_result).error());
  }

  auto item = std::move(item_result).value();
  if (auto* tag_item = absl::get_if<TagItem>(&item)) {
    // The #EXTM3U tag must be the first line in the playlist
    if (tag_item->name != ToTagName(CommonTagName::kM3u) ||
        tag_item->content.Line() != 1) {
      return ParseStatusCode::kPlaylistMissingM3uTag;
    }

    // Make sure the M3U tag parses correctly
    auto result = M3uTag::Parse(*tag_item);
    if (result.has_error()) {
      return ParseStatus(ParseStatusCode::kPlaylistMissingM3uTag)
          .AddCause(std::move(result).error());
    }

    return result;
  }

  return ParseStatusCode::kPlaylistMissingM3uTag;
}

void HandleUnknownTag(TagItem /*tag*/) {
  // Unknown tags are ignored for forward-compatibility purposes.
  // TODO(crbug.com/1266991): Should record a metric to discover common
  // unrecognized tags.
}

absl::optional<ParseStatus> ParseCommonTag(TagItem tag,
                                           CommonParserState* state) {
  DCHECK(GetTagKind(tag.name) == TagKind::kCommonTag);

  switch (static_cast<CommonTagName>(tag.name)) {
    case CommonTagName::kM3u:
      // This tag is meant to occur on the first line (which we've already
      // checked), however the spec does not explicitly regard this as an
      // error if it appears elsewhere as well.
      DCHECK(tag.content.Line() != 1);
      break;
    case CommonTagName::kXVersion: {
      auto error = ParseUniqueTag(tag, state->version_tag);
      if (error.has_value()) {
        return error;
      }

      // Max supported playlist version is 10
      if (state->version_tag->version > 10) {
        return ParseStatusCode::kPlaylistHasUnsupportedVersion;
      }
      break;
    }
    case CommonTagName::kXIndependentSegments: {
      return ParseUniqueTag(tag, state->independent_segments_tag);
    }
    case CommonTagName::kXDefine:
      // TODO(crbug.com/1266991): Implement variable substitution.
      break;
  }

  return absl::nullopt;
}

ParseStatus::Or<GURL> ParseUri(UriItem item, const GURL& playlist_uri) {
  auto resolved_uri = playlist_uri.Resolve(item.content.Str());
  if (!resolved_uri.is_valid()) {
    return ParseStatusCode::kInvalidUri;
  }

  return resolved_uri;
}

}  // namespace media::hls
