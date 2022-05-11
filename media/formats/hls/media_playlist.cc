// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace media::hls {

MediaPlaylist::MediaPlaylist(MediaPlaylist&&) = default;

MediaPlaylist& MediaPlaylist::operator=(MediaPlaylist&&) = default;

MediaPlaylist::~MediaPlaylist() = default;

ParseStatus::Or<MediaPlaylist> MediaPlaylist::Parse(
    base::StringPiece source,
    GURL uri,
    const MultivariantPlaylist* parent_playlist) {
  if (!uri.is_valid()) {
    return ParseStatusCode::kInvalidUri;
  }

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
  absl::optional<XTargetDurationTag> target_duration_tag;
  absl::optional<InfTag> inf_tag;
  absl::optional<XGapTag> gap_tag;
  absl::optional<XDiscontinuityTag> discontinuity_tag;
  absl::optional<XPlaylistTypeTag> playlist_type_tag;
  absl::optional<XEndListTag> end_list_tag;
  absl::optional<XIFramesOnlyTag> i_frames_only_tag;
  absl::optional<XMediaSequenceTag> media_sequence_tag;
  std::vector<MediaSegment> segments;

  // If this media playlist was found through a multivariant playlist, it may
  // import variables from that playlist.
  if (parent_playlist) {
    common_state.parent_variable_dict =
        &parent_playlist->GetVariableDictionary();
  }

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
        case MediaPlaylistTagName::kXTargetDuration: {
          auto error = ParseUniqueTag(*tag, target_duration_tag);
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
        case MediaPlaylistTagName::kXEndList: {
          auto error = ParseUniqueTag(*tag, end_list_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXIFramesOnly: {
          auto error = ParseUniqueTag(*tag, i_frames_only_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXPlaylistType: {
          auto error = ParseUniqueTag(*tag, playlist_type_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXMediaSequence: {
          // This tag must appear before any media segment
          if (!segments.empty()) {
            return ParseStatusCode::kMediaSegmentBeforeMediaSequenceTag;
          }

          auto error = ParseUniqueTag(*tag, media_sequence_tag);
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

    // The media sequence number of this segment can be calculated by the value
    // given by `EXT-X-MEDIA-SEQUENCE:n` (or 0), plus the number of prior
    // segments in this playlist. It's an error for the EXT-X-MEDIA-SEQUENCE
    // tag to appear after the first media segment (handled above).
    const types::DecimalInteger media_sequence_number =
        (media_sequence_tag ? media_sequence_tag->number : 0) + segments.size();

    segments.emplace_back(inf_tag->duration, media_sequence_number,
                          std::move(segment_uri), discontinuity_tag.has_value(),
                          gap_tag.has_value());

    // Reset per-segment tags
    inf_tag.reset();
    gap_tag.reset();
    discontinuity_tag.reset();
  }

  if (!target_duration_tag.has_value()) {
    return ParseStatusCode::kMediaPlaylistMissingTargetDuration;
  }

  // Ensure that no segment exceeds the target duration
  for (const auto& segment : segments) {
    const auto duration =
        static_cast<types::DecimalInteger>(std::round(segment.GetDuration()));
    if (duration > target_duration_tag->duration) {
      return ParseStatusCode::kMediaSegmentExceedsTargetDuration;
    }
  }

  // Multivariant playlists may use the `EXT-X-INDEPENDENT-SEGMENTS` tag to
  // indicate that every media playlist has independent segments. If that was
  // the case, apply that to this playlist (this does not go in reverse).
  // Otherwise, that property depends on whether that tag occurred in this
  // playlist.
  const bool independent_segments =
      common_state.independent_segments_tag.has_value() ||
      (parent_playlist && parent_playlist->AreSegmentsIndependent());

  absl::optional<PlaylistType> playlist_type;
  if (playlist_type_tag) {
    playlist_type = playlist_type_tag->type;
  }

  return MediaPlaylist(
      std::move(uri), common_state.GetVersion(), independent_segments,
      base::Seconds(target_duration_tag->duration), std::move(segments),
      playlist_type, end_list_tag.has_value(), i_frames_only_tag.has_value(),
      media_sequence_tag.has_value());
}

MediaPlaylist::MediaPlaylist(GURL uri,
                             types::DecimalInteger version,
                             bool independent_segments,
                             base::TimeDelta target_duration,
                             std::vector<MediaSegment> segments,
                             absl::optional<PlaylistType> playlist_type,
                             bool end_list,
                             bool i_frames_only,
                             bool has_media_sequence_tag)
    : Playlist(std::move(uri), version, independent_segments),
      target_duration_(target_duration),
      segments_(std::move(segments)),
      playlist_type_(playlist_type),
      end_list_(end_list),
      i_frames_only_(i_frames_only),
      has_media_sequence_tag_(has_media_sequence_tag) {
  base::TimeDelta duration;
  for (const auto& segment : segments_) {
    duration += base::Seconds(segment.GetDuration());
  }

  computed_duration_ = duration;
}

}  // namespace media::hls
