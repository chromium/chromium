// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist.h"

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "media/formats/hls/variant_stream.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace media::hls {

namespace {

// Helper for either getting or creating a rendition group, given an ID.
// This allows referencing rendition groups before they've been created.
template <typename T>
T* GetOrCreateRenditionGroup(
    base::PassKey<MultivariantPlaylist> pass_key,
    base::flat_map<std::string_view, scoped_refptr<T>>& groups,
    std::string_view id) {
  auto iter = groups.find(id);

  // If the group wasn't found, create it.
  if (iter == groups.end()) {
    auto group =
        base::MakeRefCounted<AudioRenditionGroup>(pass_key, std::string(id));
    iter = groups.insert(std::make_pair(id, std::move(group))).first;
  }

  return iter->second.get();
}

}  // namespace

MultivariantPlaylist::~MultivariantPlaylist() = default;

Playlist::Kind MultivariantPlaylist::GetKind() const {
  return Kind::kMultivariantPlaylist;
}

// static
ParseStatus::Or<scoped_refptr<MultivariantPlaylist>>
MultivariantPlaylist::Parse(std::string_view source,
                            GURL uri,
                            types::DecimalInteger version,
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
  std::optional<XStreamInfTag> inf_tag;
  std::vector<VariantStream> variants;
  base::flat_map<std::string_view, scoped_refptr<AudioRenditionGroup>>
      audio_rendition_groups;

  // Get variants out of the playlist
  while (true) {
    auto item_result = GetNextLineItem(&src_iter);
    if (!item_result.has_value()) {
      auto error = std::move(item_result).error();

      // Only tolerated error is EOF
      if (error.code() == ParseStatusCode::kReachedEOF) {
        break;
      }

      return std::move(error);
    }

    auto item = std::move(item_result).value();

    // Handle tags
    if (auto* tag = absl::get_if<TagItem>(&item)) {
      // The HLS spec requires that there may be no tags between the
      // X-STREAM-INF tag and its URI.
      if (inf_tag.has_value()) {
        return ParseStatusCode::kXStreamInfTagNotFollowedByUri;
      }

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
        case TagKind::kMediaPlaylistTag:
          return ParseStatusCode::kMultivariantPlaylistHasMediaPlaylistTag;
        case TagKind::kMultivariantPlaylistTag:
          // Handled below
          break;
      }

      switch (static_cast<MultivariantPlaylistTagName>(*tag->GetName())) {
        case MultivariantPlaylistTagName::kXContentSteering: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kContentSteering);
          }
          // TODO(crbug.com/40057824): Implement the EXT-X-CONTENT-STEERING tag
          break;
        }
        case MultivariantPlaylistTagName::kXIFrameStreamInf: {
          // TODO(crbug.com/40057824): Implement the EXT-X-I-FRAME-STREAM-INF
          // tag
          break;
        }
        case MultivariantPlaylistTagName::kXMedia: {
          auto result =
              XMediaTag::Parse(*tag, common_state.variable_dict, sub_buffer);
          if (!result.has_value()) {
            return std::move(result).error();
          }
          auto media_tag = std::move(result).value();

          switch (media_tag.type) {
            case MediaType::kAudio: {
              auto* group = GetOrCreateRenditionGroup(
                  {}, audio_rendition_groups, media_tag.group_id.Str());
              auto rendition_result =
                  group->AddRendition(base::PassKey<MultivariantPlaylist>(),
                                      std::move(media_tag), uri);
              if (!rendition_result.has_value()) {
                return std::move(rendition_result).error();
              }
              break;
            }
            case MediaType::kVideo: {
              // TODO(crbug.com/40057824): Support alternate video renditions
              break;
            }
            case MediaType::kSubtitles: {
              // TODO(crbug.com/40057824): Support subtitle renditions
              break;
            }
            case MediaType::kClosedCaptions: {
              // TODO(crbug.com/40057824): Support closed captions renditions
              break;
            }
          }
          break;
        }
        case MultivariantPlaylistTagName::kXSessionData: {
          // TODO(crbug.com/40057824): Implement the EXT-X-SESSION-DATA tag
          break;
        }
        case MultivariantPlaylistTagName::kXSessionKey: {
          if (tag_recorder) {
            tag_recorder->SetMetric(TagRecorder::Metric::kSessionKey);
          }
          // TODO(crbug.com/40057824): Implement the EXT-X-SESSION-KEY tag
          break;
        }
        case MultivariantPlaylistTagName::kXStreamInf: {
          auto error = ParseUniqueTag(*tag, inf_tag, common_state.variable_dict,
                                      sub_buffer);
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
    auto variant_uri_result = ParseUri(absl::get<UriItem>(std::move(item)), uri,
                                       common_state, sub_buffer);
    if (!variant_uri_result.has_value()) {
      return std::move(variant_uri_result).error();
    }
    auto variant_uri = std::move(variant_uri_result).value();

    // For this to be a valid variant, we must have previously parsed an
    // X-STREAM-INF tag.
    if (!inf_tag.has_value()) {
      return ParseStatusCode::kVariantMissingStreamInfTag;
    }

    scoped_refptr<AudioRenditionGroup> audio_renditions;
    if (inf_tag->audio.has_value()) {
      audio_renditions = GetOrCreateRenditionGroup({}, audio_rendition_groups,
                                                   inf_tag->audio->Str());
    }

    std::optional<std::string> video_rendition_group_name;
    if (inf_tag->video.has_value()) {
      video_rendition_group_name = std::string(inf_tag->video->Str());
    }

    variants.emplace_back(std::move(variant_uri), inf_tag->bandwidth,
                          inf_tag->average_bandwidth, inf_tag->score,
                          std::move(inf_tag->codecs), inf_tag->resolution,
                          inf_tag->frame_rate, std::move(audio_renditions),
                          std::move(video_rendition_group_name));

    // Reset per-variant tags
    inf_tag.reset();
  }

  if (inf_tag.has_value()) {
    return ParseStatusCode::kXStreamInfTagNotFollowedByUri;
  }

  // Version must match what was expected.
  if (!common_state.CheckVersion(version)) {
    return ParseStatusCode::kPlaylistHasVersionMismatch;
  }

  // Ensure that each rendition group has at least one rendition
  // If there were none, then a variant stream referenced a group that does not
  // exist. The inverse (a rendition group that was not referenced by any
  // variant) is not considered an error.
  for (const auto& group : audio_rendition_groups) {
    if (group.second->GetRenditions().empty()) {
      return ParseStatusCode::kRenditionGroupDoesNotExist;
    }
  }

  return base::MakeRefCounted<MultivariantPlaylist>(
      base::PassKey<MultivariantPlaylist>(), std::move(uri), version,
      common_state.independent_segments_tag.has_value(), std::move(variants),
      std::move(common_state.variable_dict));
}

MultivariantPlaylist::MultivariantPlaylist(
    base::PassKey<MultivariantPlaylist>,
    GURL uri,
    types::DecimalInteger version,
    bool independent_segments,
    std::vector<VariantStream> variants,
    VariableDictionary variable_dictionary)
    : Playlist(std::move(uri), version, independent_segments),
      variants_(std::move(variants)),
      variable_dictionary_(std::move(variable_dictionary)) {}

}  // namespace media::hls
