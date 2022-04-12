// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include "base/notreached.h"
#include "base/time/time.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "url/gurl.h"

namespace media::hls {

MediaPlaylist::MediaPlaylist(MediaPlaylist&&) = default;

MediaPlaylist& MediaPlaylist::operator=(MediaPlaylist&&) = default;

MediaPlaylist::~MediaPlaylist() = default;

ParseStatus::Or<MediaPlaylist> MediaPlaylist::Parse(base::StringPiece source,
                                                    GURL uri) {
  CHECK(uri.is_valid());

  SourceLineIterator src_iter{source};

  // Parse the first line of the playlist. This must be an M3U tag.
  {
    auto m3u_tag_result = CheckM3uTag(&src_iter);
    if (m3u_tag_result.has_error()) {
      return std::move(m3u_tag_result).error();
    }
  }

  CommonParserState common_state;
  VariableDictionary::SubstitutionBuffer sub_buffer;
  absl::optional<InfTag> inf_tag;
  absl::optional<XGapTag> gap_tag;
  absl::optional<XDiscontinuityTag> discontinuity_tag;
  absl::optional<XPlaylistTypeTag> playlist_type_tag;
  std::vector<MediaSegment> segments;

  // Get segments out of the playlist
  while (true) {
    auto item_result = GetNextLineItem(&src_iter);
    if (item_result.has_error()) {
      auto error = std::move(item_result).error();

      // Only tolerated error is EOF, in which case we're done.
      if (error.code() == ParseStatusCode::kReachedEOF) {
        break;
      }

      return std::move(error);
    }

    auto item = std::move(item_result).value();

    // Handle tags
    if (auto* tag = absl::get_if<TagItem>(&item)) {
      if (!tag->GetName().has_value()) {
        HandleUnknownTag(*tag);
        continue;
      }
      switch (GetTagKind(*tag->GetName())) {
        case TagKind::kCommonTag: {
          auto error = ParseCommonTag(*tag, &common_state);
          if (error.has_value()) {
            return std::move(error).value();
          }
          continue;
        }
        case TagKind::kMultivariantPlaylistTag:
          return ParseStatusCode::kMediaPlaylistHasMultivariantPlaylistTag;
        case TagKind::kMediaPlaylistTag:
          // Handled below
          break;
      }

      switch (static_cast<MediaPlaylistTagName>(*tag->GetName())) {
        case MediaPlaylistTagName::kInf: {
          auto error = ParseUniqueTag(*tag, inf_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXDiscontinuity: {
          auto error = ParseUniqueTag(*tag, discontinuity_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXGap: {
          auto error = ParseUniqueTag(*tag, gap_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXEndList:
          // TODO(crbug.com/1266991): Implement the #EXT-X-END-LIST Tag
          break;
        case MediaPlaylistTagName::kXIFramesOnly:
          // TODO(crbug.com/1266991): Implement the #EXT-X-I-FRAMES-ONLY tag
          break;
        case MediaPlaylistTagName::kXPlaylistType: {
          auto error = ParseUniqueTag(*tag, playlist_type_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
      }

      continue;
    }

    // Handle URIs
    // `GetNextLineItem` should return either a TagItem (handled above) or a
    // UriItem.
    static_assert(absl::variant_size<GetNextLineItemResult>() == 2);
    auto segment_uri_result = ParseUri(absl::get<UriItem>(std::move(item)), uri,
                                       common_state, sub_buffer);
    if (segment_uri_result.has_error()) {
      return std::move(segment_uri_result).error();
    }
    auto segment_uri = std::move(segment_uri_result).value();

    // For this to be a valid media segment, we must have parsed an Inf tag
    // since the last segment.
    if (!inf_tag.has_value()) {
      return ParseStatusCode::kMediaSegmentMissingInfTag;
    }

    segments.emplace_back(inf_tag->duration, std::move(segment_uri),
                          discontinuity_tag.has_value(), gap_tag.has_value());

    // Reset per-segment tags
    inf_tag.reset();
    gap_tag.reset();
    discontinuity_tag.reset();
  }

  absl::optional<PlaylistType> playlist_type;
  if (playlist_type_tag) {
    playlist_type = playlist_type_tag->type;
  }

  return MediaPlaylist(std::move(uri), common_state.GetVersion(),
                       common_state.independent_segments_tag.has_value(),
                       std::move(segments), playlist_type);
}

MediaPlaylist::MediaPlaylist(GURL uri,
                             types::DecimalInteger version,
                             bool independent_segments,
                             std::vector<MediaSegment> segments,
                             absl::optional<PlaylistType> playlist_type)
    : Playlist(std::move(uri), version, independent_segments),
      segments_(std::move(segments)),
      playlist_type_(playlist_type) {
  base::TimeDelta duration;
  for (const auto& segment : segments_) {
    duration += base::Seconds(segment.GetDuration());
  }

  computed_duration_ = duration;
}

}  // namespace media::hls
