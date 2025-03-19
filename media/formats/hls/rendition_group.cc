// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_group.h"

#include <optional>
#include <variant>

#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/quirks.h"
#include "media/formats/hls/rendition.h"
#include "media/formats/hls/tags.h"

namespace media::hls {

RenditionGroup::RenditionGroup(base::PassKey<MultivariantPlaylist>,
                               std::string id)
    : id_(std::move(id)) {}

RenditionGroup::~RenditionGroup() = default;

ParseStatus::Or<std::monostate> RenditionGroup::AddRendition(
    base::PassKey<MultivariantPlaylist>,
    XMediaTag tag,
    const GURL& playlist_uri) {
  DCHECK(tag.group_id.Str() == id_);
  DCHECK(playlist_uri.is_valid());

  // INSTREAM-ID MUST NOT be specified for any rendition with a TYPE other than
  // CLOSED-CAPTIONS. We don't support CLOSED-CAPTIONS.
  DCHECK(tag.instream_id == std::nullopt);

  // FORCED MUST NOT be present unless the TYPE is SUBTITLES. We don't support
  // the SUBTITLES type.
  DCHECK(tag.forced == false);

  std::optional<GURL> uri;
  if (tag.uri.has_value()) {
    uri = playlist_uri.Resolve(tag.uri->Str());
    if (!uri->is_valid()) {
      return ParseStatusCode::kInvalidUri;
    }
  }

  auto name = std::string(tag.name.Str());
  if (!HLSQuirks::DeduplicateRenditionNamesInGroup() &&
      renditions_map_.contains(name)) {
    return ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames;
  }

  while (renditions_map_.contains(name)) {
    // TODO(crbug.com/395949828): According to the spec:
    // "All EXT-X-MEDIA tags in the same Group MUST have different NAME
    // attributes."
    // However, it's fairly common for this to not be the case on the web at
    // large, and safari's implementation will accept manifests with duplicate
    // rendition names in a single group. We don't really use the name outside
    // of the key for storage, so we can just append a number to it and be ok.
    name += '0';
  }

  std::optional<std::string> language;
  if (tag.language.has_value()) {
    language = std::string(tag.language->Str());
  }

  std::optional<std::string> associated_language;
  if (tag.associated_language.has_value()) {
    associated_language = std::string(tag.associated_language->Str());
  }

  auto& rendition = renditions_.emplace_back(
      base::PassKey<RenditionGroup>(),
      Rendition::CtorArgs{
          .uri = std::move(uri),
          .name = name,
          .language = std::move(language),
          .associated_language = std::move(associated_language),
          .stable_rendition_id = std::move(tag.stable_rendition_id),
          .channels = std::move(tag.channels),
          .autoselect = std::move(tag.autoselect),
      });
  renditions_map_.emplace(std::move(name), &rendition);

  if (tag.is_default) {
    if (!default_rendition_) {
      default_rendition_ = &rendition;
    } else {
      // TODO(crbug.com/40057824): According to the spec there "MUST" be
      // no more than a single rendition per-group with DEFAULT=YES, but some of
      // Apple's own presentations break this rule. Ex:
      // https://events-delivery.apple.com/0205eyyhwbbqexozkwmgccegwnjyrktg/m3u8/vod_index-dpyfrsVksFWjneFiptbXnAMYBtGYbXeZ.m3u8
      // Could potentially use the CHARACTERISTICS tag as a heuristic for when
      // to ignore this. For now, we ignore this to maximize compatibility with
      // existing playlists.
    }
  }

  return std::monostate();
}

}  // namespace media::hls
