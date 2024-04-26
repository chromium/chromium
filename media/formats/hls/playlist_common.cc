// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/playlist_common.h"

#include "base/notreached.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/types.h"

namespace media::hls {

bool CommonParserState::CheckVersion(
    types::DecimalInteger expected_version) const {
  if (version_tag.has_value()) {
    return expected_version == version_tag->version;
  } else {
    return expected_version == Playlist::kDefaultVersion;
  }
}

ParseStatus::Or<M3uTag> CheckM3uTag(SourceLineIterator* src_iter) {
  auto item_result = GetNextLineItem(src_iter);
  if (!item_result.has_value()) {
    return ParseStatus(ParseStatusCode::kPlaylistMissingM3uTag)
        .AddCause(std::move(item_result).error());
  }

  auto item = std::move(item_result).value();
  if (auto* tag_item = absl::get_if<TagItem>(&item)) {
    // The #EXTM3U tag must be the first line in the playlist
    if (tag_item->GetName() != ToTagName(CommonTagName::kM3u) ||
        tag_item->GetLineNumber() != 1) {
      return ParseStatusCode::kPlaylistMissingM3uTag;
    }

    // Make sure the M3U tag parses correctly
    auto result = M3uTag::Parse(*tag_item);
    if (!result.has_value()) {
      return ParseStatus(ParseStatusCode::kPlaylistMissingM3uTag)
          .AddCause(std::move(result).error());
    }

    return result;
  }

  return ParseStatusCode::kPlaylistMissingM3uTag;
}

void HandleUnknownTag(TagItem /*tag*/) {
  // Unknown tags are ignored for forward-compatibility purposes.
  // TODO(crbug.com/40057824): Should record a metric to discover common
  // unrecognized tags.
}

std::optional<ParseStatus> ParseCommonTag(TagItem tag,
                                          CommonParserState* state) {
  DCHECK(tag.GetName() && GetTagKind(*tag.GetName()) == TagKind::kCommonTag);

  switch (static_cast<CommonTagName>(*tag.GetName())) {
    case CommonTagName::kM3u: {
      // This tag is meant to occur on the first line (which we've already
      // checked), however the spec does not explicitly regard this as an
      // error if it appears elsewhere as well.
      DCHECK(tag.GetLineNumber() != 1);
      break;
    }
    case CommonTagName::kXDefine: {
      auto tag_result = XDefineTag::Parse(tag);
      if (!tag_result.has_value()) {
        return std::move(tag_result).error();
      }
      auto tag_value = std::move(tag_result).value();

      // Imported variables have a null `value` member. If that's the case, look
      // up the value in the parent playlist dictionary.
      if (!tag_value.value) {
        if (!state->parent_variable_dict) {
          return ParseStatusCode::kImportedVariableInParentlessPlaylist;
        }

        auto value = state->parent_variable_dict->Find(tag_value.name);
        if (!value) {
          return ParseStatusCode::kImportedVariableUndefined;
        }

        tag_value.value = *value;
      }

      // Insert the definition, ensuring it has not been defined twice
      if (!state->variable_dict.Insert(tag_value.name,
                                       std::string{*tag_value.value})) {
        return ParseStatusCode::kVariableDefinedMultipleTimes;
      }
      break;
    }
    case CommonTagName::kXIndependentSegments: {
      return ParseUniqueTag(tag, state->independent_segments_tag);
    }
    case CommonTagName::kXStart: {
      // TODO(crbug.com/40057824): Implement the EXT-X-START tag.
      break;
    }
    case CommonTagName::kXVersion: {
      auto error = ParseUniqueTag(tag, state->version_tag);
      if (error.has_value()) {
        return error;
      }
      break;
    }
  }

  return std::nullopt;
}

ParseStatus::Or<GURL> ParseUri(
    UriItem item,
    const GURL& playlist_uri,
    const CommonParserState& state,
    VariableDictionary::SubstitutionBuffer& sub_buffer) {
  // Variables may appear in URIs, check for any occurrences and resolve them.
  auto uri_str_result = state.variable_dict.Resolve(item.content, sub_buffer);
  if (!uri_str_result.has_value()) {
    return std::move(uri_str_result).error();
  }

  // URIs may be relative to the playlist URI, resolve it against that.
  auto resolved_uri =
      playlist_uri.Resolve(std::move(uri_str_result).value().Str());
  if (!resolved_uri.is_valid()) {
    return ParseStatusCode::kInvalidUri;
  }

  return resolved_uri;
}

}  // namespace media::hls
