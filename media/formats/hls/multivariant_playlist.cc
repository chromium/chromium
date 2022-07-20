// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/multivariant_playlist.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/items.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/source_string.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "media/formats/hls/variant_stream.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace media::hls {

MultivariantPlaylist::MultivariantPlaylist(MultivariantPlaylist&&) = default;

MultivariantPlaylist& MultivariantPlaylist::operator=(MultivariantPlaylist&&) =
    default;

MultivariantPlaylist::~MultivariantPlaylist() = default;

// static
ParseStatus::Or<MultivariantPlaylist> MultivariantPlaylist::Parse(
    base::StringPiece source,
    GURL uri) {
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
  absl::optional<XStreamInfTag> inf_tag;
  std::vector<VariantStream> variants;

  // Get variants out of the playlist
  while (true) {
    auto item_result = GetNextLineItem(&src_iter);
    if (item_result.has_error()) {
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
          // TODO(crbug.com/1266991): Implement the EXT-X-CONTENT-STEERING tag
          break;
        }
        case MultivariantPlaylistTagName::kXIFrameStreamInf: {
          // TODO(crbug.com/1266991): Implement the EXT-X-I-FRAME-STREAM-INF tag
          break;
        }
        case MultivariantPlaylistTagName::kXMedia: {
          // TODO(crbug.com/1266991): Implement the EXT-X-MEDIA tag
          break;
        }
        case MultivariantPlaylistTagName::kXSessionData: {
          // TODO(crbug.com/1266991): Implement the EXT-X-SESSION-DATA tag
          break;
        }
        case MultivariantPlaylistTagName::kXSessionKey: {
          // TODO(crbug.com/1266991): Implement the EXT-X-SESSION-KEY tag
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
    if (variant_uri_result.has_error()) {
      return std::move(variant_uri_result).error();
    }
    auto variant_uri = std::move(variant_uri_result).value();

    // For this to be a valid variant, we must have previously parsed an
    // X-STREAM-INF tag.
    if (!inf_tag.has_value()) {
      return ParseStatusCode::kVariantMissingStreamInfTag;
    }

    variants.emplace_back(std::move(variant_uri), inf_tag->bandwidth,
                          inf_tag->average_bandwidth, inf_tag->score,
                          std::move(inf_tag->codecs), inf_tag->resolution,
                          inf_tag->frame_rate);

    // Reset per-variant tags
    inf_tag.reset();
  }

  if (inf_tag.has_value()) {
    return ParseStatusCode::kXStreamInfTagNotFollowedByUri;
  }

  return MultivariantPlaylist(std::move(uri), common_state.GetVersion(),
                              common_state.independent_segments_tag.has_value(),
                              std::move(variants),
                              std::move(common_state.variable_dict));
}

MultivariantPlaylist::MultivariantPlaylist(
    GURL uri,
    types::DecimalInteger version,
    bool independent_segments,
    std::vector<VariantStream> variants,
    VariableDictionary variable_dictionary)
    : Playlist(std::move(uri), version, independent_segments),
      variants_(std::move(variants)),
      variable_dictionary_(std::move(variable_dictionary)) {}

}  // namespace media::hls
