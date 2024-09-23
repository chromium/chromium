// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/media_playlist.h"

#include <cmath>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/numerics/clamped_math.h"
#include "base/time/time.h"
#include "media/formats/hls/media_segment.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace media::hls {

struct MediaPlaylist::CtorArgs {
  GURL uri;
  types::DecimalInteger version;
  bool independent_segments;
  base::TimeDelta target_duration;
  std::optional<PartialSegmentInfo> partial_segment_info;
  std::vector<scoped_refptr<MediaSegment>> segments;
  base::TimeDelta total_duration;
  std::optional<PlaylistType> playlist_type;
  bool end_list;
  bool i_frames_only;
  bool has_media_sequence_tag;
  bool can_skip_dateranges;
  bool can_block_reload;
  std::optional<base::TimeDelta> skip_boundary;
  base::TimeDelta hold_back_distance;
  std::optional<base::TimeDelta> part_hold_back_distance;
};

MediaPlaylist::~MediaPlaylist() = default;

Playlist::Kind MediaPlaylist::GetKind() const {
  return Kind::kMediaPlaylist;
}

// static
ParseStatus::Or<scoped_refptr<MediaPlaylist>> MediaPlaylist::Parse(
    std::string_view source,
    GURL uri,
    types::DecimalInteger version,
    const MultivariantPlaylist* parent_playlist,
    TagRecorder* tag_recorder) {
  DCHECK(version != 0);
  if (version < Playlist::kMinSupportedVersion ||
      version > Playlist::kMaxSupportedVersion) {
    return ParseStatusCode::kPlaylistHasUnsupportedVersion;
  }

  if (!uri.is_valid()) {
    return ParseStatusCode::kInvalidUri;
  }

  SourceLineIterator src_iter{source};

  // Parse the first line of the playlist. This must be an M3U tag.
  {
    auto m3u_tag_result = CheckM3uTag(&src_iter);
    if (!m3u_tag_result.has_value()) {
      return std::move(m3u_tag_result).error();
    }
  }

  CommonParserState common_state;
  VariableDictionary::SubstitutionBuffer sub_buffer;
  std::optional<XTargetDurationTag> target_duration_tag;
  std::optional<InfTag> inf_tag;
  std::optional<XGapTag> gap_tag;
  std::optional<XDiscontinuityTag> discontinuity_tag;
  std::optional<XByteRangeTag> byterange_tag;
  std::optional<XBitrateTag> bitrate_tag;
  std::optional<XPlaylistTypeTag> playlist_type_tag;
  std::optional<XEndListTag> end_list_tag;
  std::optional<XIFramesOnlyTag> i_frames_only_tag;
  std::optional<XPartInfTag> part_inf_tag;
  std::optional<XServerControlTag> server_control_tag;
  std::optional<XMediaSequenceTag> media_sequence_tag;
  std::optional<XDiscontinuitySequenceTag> discontinuity_sequence_tag;
  std::vector<scoped_refptr<MediaSegment>> segments;
  scoped_refptr<MediaSegment::InitializationSegment> initialization_segment;
  scoped_refptr<MediaSegment::EncryptionData> encryption_data;
  bool new_init_segment = false;
  bool new_encryption_data = false;

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
    if (!item_result.has_value()) {
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
        if (tag_recorder) {
          tag_recorder->SetMetric(TagRecorder::Metric::kUnknownTag);
        }
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
        case MediaPlaylistTagName::kXBitrate: {
          auto result = XBitrateTag::Parse(*tag);
          if (!result.has_value()) {
            return std::move(result).error();
          }
          bitrate_tag = std::move(result).value();
          break;
        }
        case MediaPlaylistTagName::kXByteRange: {
          // TODO(crbug.com/40226468): Investigate supporting aspects of
          // this tag not described by the spec
          auto error = ParseUniqueTag(*tag, byterange_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXDateRange: {
          // TODO(crbug.com/40057824): Implement the EXT-X-DATERANGE tag.
          break;
        }
        case MediaPlaylistTagName::kXDiscontinuity: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kDiscontinuity);
          }
          // Multiple occurrences of `EXT-X-DISCONTINUITY` per media segment are
          // allowed, and each increments the segment's discontinuity sequence
          // number by 1. The spec doesn't explicitly forbid this, and this
          // seems to be how other HLS clients handle this scenario.
          auto result = XDiscontinuityTag::Parse(*tag);
          if (!result.has_value()) {
            return std::move(result).error();
          }

          // Even if there was a previous discontinuity tag, overwrite the value
          // and increment the discontinuity sequence number by 1.
          discontinuity_tag = std::move(result).value();
          discontinuity_sequence_number += 1;
          break;
        }
        case MediaPlaylistTagName::kXDiscontinuitySequence: {
          if (tag_recorder) {
            tag_recorder->SetMetric(
                TagRecorder::Metric::kDiscontinuitySequence);
          }
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
        case MediaPlaylistTagName::kXEndList: {
          auto error = ParseUniqueTag(*tag, end_list_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXGap: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kGap);
          }
          auto error = ParseUniqueTag(*tag, gap_tag);
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
        case MediaPlaylistTagName::kXKey: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kKey);
          }
          auto result =
              XKeyTag::Parse(*tag, common_state.variable_dict, sub_buffer);
          if (!result.has_value()) {
            return std::move(result).error().AddHere();
          }
          auto value = std::move(result).value();

          if (tag_recorder) {
            TagRecorder::Metric crypto;
            switch (value.method) {
              case XKeyTagMethod::kNone:
                crypto = TagRecorder::Metric::kNoCrypto;
                break;
              case XKeyTagMethod::kAES128:
              case XKeyTagMethod::kAES256:
                crypto = TagRecorder::Metric::kSegmentAES;
                break;
              case XKeyTagMethod::kSampleAES:
                crypto = TagRecorder::Metric::kSample;
                break;
              case XKeyTagMethod::kSampleAESCTR:
                crypto = TagRecorder::Metric::kAESCTR;
                break;
              case XKeyTagMethod::kSampleAESCENC:
                crypto = TagRecorder::Metric::kAESCENC;
                break;
              case XKeyTagMethod::kISO230017:
                crypto = TagRecorder::Metric::kISO230017;
                break;
            }
            tag_recorder->SetMetric(crypto);
          }
          if (value.method == XKeyTagMethod::kNone) {
            if (encryption_data != nullptr) {
              new_encryption_data = true;
            }
            encryption_data = nullptr;
          } else {
            auto resource_uri = uri.Resolve(value.uri.value().Str());
            if (!resource_uri.is_valid()) {
              return ParseStatusCode::kInvalidUri;
            }
            new_encryption_data = true;
            encryption_data =
                base::MakeRefCounted<MediaSegment::EncryptionData>(
                    std::move(resource_uri), value.method, value.keyformat,
                    value.iv);
          }

          break;
        }
        case MediaPlaylistTagName::kXMap: {
          auto result =
              XMapTag::Parse(*tag, common_state.variable_dict, sub_buffer);
          if (!result.has_value()) {
            return std::move(result).error();
          }
          auto value = std::move(result).value();

          // Resolve the URI against the playlist URI
          auto resource_uri = uri.Resolve(value.uri.Str());
          if (!resource_uri.is_valid()) {
            return ParseStatusCode::kInvalidUri;
          }

          // Extract the byte range
          std::optional<types::ByteRange> byte_range;
          if (value.byte_range.has_value()) {
            // Safari defaults byte range offset to 0, do that here as well.
            byte_range = types::ByteRange::Validate(
                value.byte_range->length, value.byte_range->offset.value_or(0));
            if (!byte_range.has_value()) {
              return ParseStatusCode::kByteRangeInvalid;
            }
          }

          new_init_segment = true;
          initialization_segment =
              base::MakeRefCounted<MediaSegment::InitializationSegment>(
                  std::move(resource_uri), byte_range);
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
        case MediaPlaylistTagName::kXPart: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kPart);
          }
          // TODO(crbug.com/40057824): Integrate the EXT-X-PART tag.
          break;
        }
        case MediaPlaylistTagName::kXPartInf: {
          auto error = ParseUniqueTag(*tag, part_inf_tag);
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
        case MediaPlaylistTagName::kXPreloadHint: {
          // TODO(crbug.com/40057824): Implement the EXT-X-PRELOAD-HINT tag.
          break;
        }
        case MediaPlaylistTagName::kXProgramDateTime: {
          // TODO(crbug.com/40057824): Implement the EXT-X-PROGRAM-DATE-TIME
          // tag.
          break;
        }
        case MediaPlaylistTagName::kXRenditionReport: {
          // TODO(crbug.com/40057824): Implement the EXT-X-RENDITION-REPORT tag.
          break;
        }
        case MediaPlaylistTagName::kXServerControl: {
          auto error = ParseUniqueTag(*tag, server_control_tag);
          if (error.has_value()) {
            return std::move(error).value();
          }
          break;
        }
        case MediaPlaylistTagName::kXSkip: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kSkip);
          }
          // TODO(crbug.com/40057824): Implement the EXT-X-SKIP tag.
          // Since the appearance of the EXT-X-SKIP tag implies that this is a
          // playlist delta update, we cannot parse this playlist.
          return ParseStatusCode::kPlaylistHasUnexpectedDeltaUpdate;
        }
        case MediaPlaylistTagName::kXTargetDuration: {
          auto error = ParseUniqueTag(*tag, target_duration_tag);
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
    if (!segment_uri_result.has_value()) {
      return std::move(segment_uri_result).error();
    }
    auto segment_uri = std::move(segment_uri_result).value();

    if (tag_recorder) {
      const auto filename = segment_uri.ExtractFileName();
      if (filename.ends_with(".ts")) {
        tag_recorder->SetMetric(TagRecorder::Metric::kSegmentTS);
      } else if (filename.ends_with(".mp4")) {
        tag_recorder->SetMetric(TagRecorder::Metric::kSegmentMP4);
      } else if (filename.ends_with(".aac")) {
        tag_recorder->SetMetric(TagRecorder::Metric::kSegmentAAC);
      } else {
        tag_recorder->SetMetric(TagRecorder::Metric::kSegmentOther);
      }
    }

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

    std::optional<types::ByteRange> byterange;
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
      } else if (!segments.back()->GetByteRange().has_value()) {
        return ParseStatusCode::kByteRangeRequiresOffset;
      } else if (segments.back()->GetUri() != segment_uri) {
        return ParseStatusCode::kByteRangeRequiresOffset;
      } else {
        offset = segments.back()->GetByteRange()->GetEnd();
      }

      byterange = types::ByteRange::Validate(range.length, offset);
      if (!byterange) {
        return ParseStatusCode::kByteRangeInvalid;
      }
    }

    // The previous occurrence of the EXT-X-BITRATE tag applies to this segment
    // only if this segment is not a byterange of its resource.
    std::optional<types::DecimalInteger> bitrate;
    if (bitrate_tag.has_value() && !byterange.has_value()) {
      // The value in the tag is expressed in kilobits per-second, but we wish
      // to normalize all bitrates to bits-per-second. The spec specifically
      // uses 'kilobit' as opposed to 'kibibit', so we multiply by 1000 instead
      // of 1024.
      // Ensure we don't overflow `DecimalInteger` when doing this
      // multiplication.
      bitrate = base::ClampMul(bitrate_tag->bitrate, 1000u);
    }

    segments.push_back(base::MakeRefCounted<MediaSegment>(
        inf_tag->duration, media_sequence_number, discontinuity_sequence_number,
        std::move(segment_uri), initialization_segment, encryption_data,
        byterange, bitrate, discontinuity_tag.has_value(), gap_tag.has_value(),
        new_init_segment, new_encryption_data));
    new_init_segment = false;
    new_encryption_data = false;

    // Reset per-segment tags
    inf_tag.reset();
    gap_tag.reset();
    discontinuity_tag.reset();
    byterange_tag.reset();
  }

  // Version must match what was expected.
  if (!common_state.CheckVersion(version)) {
    return ParseStatusCode::kPlaylistHasVersionMismatch;
  }

  if (!target_duration_tag.has_value()) {
    return ParseStatusCode::kMediaPlaylistMissingTargetDuration;
  }
  const auto target_duration = target_duration_tag->duration;
  if (target_duration > kMaxTargetDuration) {
    return ParseStatusCode::kTargetDurationExceedsMax;
  }

  std::optional<PartialSegmentInfo> partial_segment_info;
  if (part_inf_tag.has_value()) {
    partial_segment_info = MediaPlaylist::PartialSegmentInfo{
        .target_duration = part_inf_tag->target_duration};

    // Since the combination of partial segments should be equivalent to their
    // parent segment, the partial segment target duration should not exceed the
    // parent segment target duration.
    if (partial_segment_info->target_duration > target_duration) {
      return ParseStatusCode::kPartTargetDurationExceedsTargetDuration;
    }
  }

  bool can_skip_dateranges = false;
  bool can_block_reload = false;
  std::optional<base::TimeDelta> skip_boundary;
  base::TimeDelta hold_back_distance = target_duration * 3;
  std::optional<base::TimeDelta> part_hold_back_distance;
  if (server_control_tag.has_value()) {
    can_skip_dateranges = server_control_tag->can_skip_dateranges;
    can_block_reload = server_control_tag->can_block_reload;

    if (server_control_tag->skip_boundary.has_value()) {
      skip_boundary = server_control_tag->skip_boundary.value();

      // The skip boundary MUST be at least six times the target
      // duration.
      if (skip_boundary.value() < target_duration * 6) {
        return ParseStatusCode::kSkipBoundaryTooLow;
      }
    }

    if (server_control_tag->hold_back.has_value()) {
      hold_back_distance = server_control_tag->hold_back.value();

      // The hold back distance MUST be at least three times the target
      // duration.
      if (hold_back_distance < target_duration * 3) {
        return ParseStatusCode::kHoldBackDistanceTooLow;
      }
    }

    if (server_control_tag->part_hold_back.has_value()) {
      part_hold_back_distance = server_control_tag->part_hold_back.value();

      // The part hold back distance MUST be at least twice the part target
      // duration.
      if (partial_segment_info.has_value() &&
          part_hold_back_distance < partial_segment_info->target_duration * 2) {
        return ParseStatusCode::kPartHoldBackDistanceTooLow;
      }
    }
  }

  // PART-HOLD-BACK is required if the PART-INF tag appeared
  if (part_inf_tag.has_value() && !part_hold_back_distance.has_value()) {
    return ParseStatusCode::kPartInfTagWithoutPartHoldBack;
  }

  // Ensure that no segment exceeds the target duration
  base::TimeDelta total_duration;
  for (const auto& segment : segments) {
    // The spec says that the segment duration should not exceed the target
    // duration after rounding to the nearest integer.
    // https://datatracker.ietf.org/doc/html/draft-pantos-hls-rfc8216bis#section-4.4.3.1
    const auto rounded_duration =
        std::round(segment->GetDuration().InSecondsF());

    // Compare the rounded segment duration to the target duration (as an
    // integer). Target duration should always be an integer of seconds, so to
    // avoid floating-point precision issues we use `InSeconds()` rather than
    // `InSecondsF()`.
    if (rounded_duration > target_duration.InSeconds()) {
      return ParseStatusCode::kMediaSegmentExceedsTargetDuration;
    }

    total_duration += segment->GetDuration();
  }

  if (total_duration.is_max()) {
    return ParseStatusCode::kPlaylistOverflowsTimeDelta;
  }

  // Multivariant playlists may use the `EXT-X-INDEPENDENT-SEGMENTS` tag to
  // indicate that every media playlist has independent segments. If that was
  // the case, apply that to this playlist (this does not go in reverse).
  // Otherwise, that property depends on whether that tag occurred in this
  // playlist.
  const bool independent_segments =
      common_state.independent_segments_tag.has_value() ||
      (parent_playlist && parent_playlist->AreSegmentsIndependent());

  std::optional<PlaylistType> playlist_type;
  if (playlist_type_tag) {
    playlist_type = playlist_type_tag->type;
  }

  return base::MakeRefCounted<MediaPlaylist>(
      base::PassKey<MediaPlaylist>(),
      CtorArgs{.uri = std::move(uri),
               .version = version,
               .independent_segments = independent_segments,
               .target_duration = target_duration,
               .partial_segment_info = std::move(partial_segment_info),
               .segments = std::move(segments),
               .total_duration = total_duration,
               .playlist_type = playlist_type,
               .end_list = end_list_tag.has_value(),
               .i_frames_only = i_frames_only_tag.has_value(),
               .has_media_sequence_tag = media_sequence_tag.has_value(),
               .can_skip_dateranges = can_skip_dateranges,
               .can_block_reload = can_block_reload,
               .skip_boundary = skip_boundary,
               .hold_back_distance = hold_back_distance,
               .part_hold_back_distance = part_hold_back_distance});
}

MediaPlaylist::MediaPlaylist(base::PassKey<MediaPlaylist>, CtorArgs args)
    : Playlist(std::move(args.uri), args.version, args.independent_segments),
      target_duration_(args.target_duration),
      partial_segment_info_(std::move(args.partial_segment_info)),
      segments_(std::move(args.segments)),
      computed_duration_(args.total_duration),
      playlist_type_(args.playlist_type),
      end_list_(args.end_list),
      i_frames_only_(args.i_frames_only),
      has_media_sequence_tag_(args.has_media_sequence_tag),
      can_skip_dateranges_(args.can_skip_dateranges),
      can_block_reload_(args.can_block_reload),
      skip_boundary_(args.skip_boundary),
      hold_back_distance_(args.hold_back_distance),
      part_hold_back_distance_(args.part_hold_back_distance) {}

}  // namespace media::hls
