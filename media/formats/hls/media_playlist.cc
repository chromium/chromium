// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
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

struct MediaPlaylist::CtorArgs {
  GURL uri;
  types::DecimalInteger version;
  bool independent_segments;
  base::TimeDelta target_duration;
  absl::optional<PartialSegmentInfo> partial_segment_info;
  std::vector<MediaSegment> segments;
  absl::optional<PlaylistType> playlist_type;
  bool end_list;
  bool i_frames_only;
  bool has_media_sequence_tag;
};

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
  absl::optional<XByteRangeTag> byterange_tag;
  absl::optional<XBitrateTag> bitrate_tag;
  absl::optional<XPlaylistTypeTag> playlist_type_tag;
  absl::optional<XEndListTag> end_list_tag;
  absl::optional<XIFramesOnlyTag> i_frames_only_tag;
  absl::optional<XPartInfTag> part_inf_tag;
  absl::optional<XMediaSequenceTag> media_sequence_tag;
  absl::optional<XDiscontinuitySequenceTag> discontinuity_sequence_tag;
  std::vector<MediaSegment> segments;

  types::DecimalInteger discontinuity_sequence_number = 0;

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
          // Multiple occurrences of `EXT-X-DISCONTINUITY` per media segment are
          // allowed, and each increments the segment's discontinuity sequence
          // number by 1. The spec doesn't explicitly forbid this, and this
          // seems to be how other HLS clients handle this scenario.
          auto result = XDiscontinuityTag::Parse(*tag);
          if (result.has_error()) {
            return std::move(result).error();
          }

          // Even if there was a previous discontinuity tag, overwrite the value
          // and increment the discontinuity sequence number by 1.
          discontinuity_tag = std::move(result).value();
          discontinuity_sequence_number += 1;
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
        case MediaPlaylistTagName::kXPartInf: {
          auto error = ParseUniqueTag(*tag, part_inf_tag);
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
        case MediaPlaylistTagName::kXDiscontinuitySequence: {
          auto error = ParseUniqueTag(*tag, discontinuity_sequence_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }

          // This tag must appear before any media segment or
          // EXT-X-DISCONTINUITY tag.
          if (!segments.empty()) {
            return ParseStatusCode::kMediaSegmentBeforeDiscontinuitySequenceTag;
          }
          if (discontinuity_sequence_number != 0) {
            return ParseStatusCode::
                kDiscontinuityTagBeforeDiscontinuitySequenceTag;
          }

          discontinuity_sequence_number = discontinuity_sequence_tag->number;
          break;
        }
        case MediaPlaylistTagName::kXByteRange: {
          // TODO(https://crbug.com/1328528): Investigate supporting aspects of
          // this tag not described by the spec
          auto error = ParseUniqueTag(*tag, byterange_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXBitrate: {
          auto result = XBitrateTag::Parse(*tag);
          if (result.has_error()) {
            return std::move(result).error();
          }
          bitrate_tag = std::move(result).value();
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

    absl::optional<types::ByteRange> byterange;
    if (byterange_tag.has_value()) {
      auto range = byterange_tag->range;

      // If this media segment had an EXT-X-BYTERANGE tag without an offset, the
      // previous media segment must have been a byterange of the same resource.
      // In that case, the offset is that of the byte following the previous
      // media segment.
      types::DecimalInteger offset;
      if (range.offset.has_value()) {
        offset = range.offset.value();
      } else if (segments.empty()) {
        return ParseStatusCode::kByteRangeRequiresOffset;
      } else if (!segments.back().GetByteRange().has_value()) {
        return ParseStatusCode::kByteRangeRequiresOffset;
      } else if (segments.back().GetUri() != segment_uri) {
        return ParseStatusCode::kByteRangeRequiresOffset;
      } else {
        offset = segments.back().GetByteRange()->GetEnd();
      }

      byterange = types::ByteRange::Validate(range.length, offset);
      if (!byterange) {
        return ParseStatusCode::kByteRangeInvalid;
      }
    }

    // The previous occurrence of the EXT-X-BITRATE tag applies to this segment
    // only if this segment is not a byterange of its resource.
    absl::optional<types::DecimalInteger> bitrate;
    if (bitrate_tag.has_value() && !byterange.has_value()) {
      // The value in the tag is expressed in kilobits per-second, but we wish
      // to normalize all bitrates to bits-per-second. The spec specifically
      // uses 'kilobit' as opposed to 'kibibit', so we multiply by 1000 instead
      // of 1024.
      // Ensure we don't overflow `DecimalInteger` when doing this
      // multiplication.
      bitrate = base::ClampMul(bitrate_tag->bitrate, 1000u);
    }

    segments.emplace_back(inf_tag->duration, media_sequence_number,
                          discontinuity_sequence_number, std::move(segment_uri),
                          byterange, bitrate, discontinuity_tag.has_value(),
                          gap_tag.has_value());

    // Reset per-segment tags
    inf_tag.reset();
    gap_tag.reset();
    discontinuity_tag.reset();
    byterange_tag.reset();
  }

  if (!target_duration_tag.has_value()) {
    return ParseStatusCode::kMediaPlaylistMissingTargetDuration;
  }

  absl::optional<PartialSegmentInfo> partial_segment_info;
  if (part_inf_tag.has_value()) {
    partial_segment_info = MediaPlaylist::PartialSegmentInfo{
        .target_duration = part_inf_tag->target_duration};
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
      CtorArgs{.uri = std::move(uri),
               .version = common_state.GetVersion(),
               .independent_segments = independent_segments,
               .target_duration = base::Seconds(target_duration_tag->duration),
               .partial_segment_info = std::move(partial_segment_info),
               .segments = std::move(segments),
               .playlist_type = playlist_type,
               .end_list = end_list_tag.has_value(),
               .i_frames_only = i_frames_only_tag.has_value(),
               .has_media_sequence_tag = media_sequence_tag.has_value()});
}

MediaPlaylist::MediaPlaylist(CtorArgs args)
    : Playlist(std::move(args.uri), args.version, args.independent_segments),
      target_duration_(args.target_duration),
      partial_segment_info_(std::move(args.partial_segment_info)),
      segments_(std::move(args.segments)),
      playlist_type_(args.playlist_type),
      end_list_(args.end_list),
      i_frames_only_(args.i_frames_only),
      has_media_sequence_tag_(args.has_media_sequence_tag) {
  base::TimeDelta duration;
  for (const auto& segment : segments_) {
    duration += base::Seconds(segment.GetDuration());
  }

  computed_duration_ = duration;
}

}  // namespace media::hls
