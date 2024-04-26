// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/audio_rendition.h"

#include <optional>

#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/tags.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace media::hls {

struct AudioRendition::CtorArgs {
  decltype(AudioRendition::uri_) uri;
  decltype(AudioRendition::name_) name;
  decltype(AudioRendition::language_) language;
  decltype(AudioRendition::associated_language_) associated_language;
  decltype(AudioRendition::stable_rendition_id_) stable_rendition_id;
  decltype(AudioRendition::channels_) channels;
  decltype(AudioRendition::autoselect_) autoselect;
};

AudioRendition::AudioRendition(base::PassKey<AudioRenditionGroup>,
                               CtorArgs args)
    : uri_(std::move(args.uri)),
      name_(std::move(args.name)),
      language_(std::move(args.language)),
      stable_rendition_id_(std::move(args.stable_rendition_id)),
      channels_(std::move(args.channels)),
      autoselect_(std::move(args.autoselect)) {}

AudioRendition::AudioRendition(AudioRendition&&) = default;

AudioRendition::~AudioRendition() = default;

AudioRenditionGroup::AudioRenditionGroup(base::PassKey<MultivariantPlaylist>,
                                         std::string id)
    : id_(std::move(id)) {}

AudioRenditionGroup::~AudioRenditionGroup() = default;

ParseStatus::Or<absl::monostate> AudioRenditionGroup::AddRendition(
    base::PassKey<MultivariantPlaylist>,
    XMediaTag tag,
    const GURL& playlist_uri) {
  DCHECK(tag.type == MediaType::kAudio);
  DCHECK(tag.instream_id == std::nullopt);
  DCHECK(tag.group_id.Str() == id_);
  DCHECK(tag.forced == false);
  DCHECK(playlist_uri.is_valid());

  std::optional<GURL> uri;
  if (tag.uri.has_value()) {
    uri = playlist_uri.Resolve(tag.uri->Str());
    if (!uri->is_valid()) {
      return ParseStatusCode::kInvalidUri;
    }
  }

  auto name = std::string(tag.name.Str());
  if (renditions_map_.contains(name)) {
    return ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames;
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
      base::PassKey<AudioRenditionGroup>(),
      AudioRendition::CtorArgs{
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

  return absl::monostate();
}

const AudioRendition* AudioRenditionGroup::GetRendition(
    std::string_view name) const {
  auto iter = renditions_map_.find(name);
  if (iter == renditions_map_.end()) {
    return nullptr;
  }

  return iter->second;
}

}  // namespace media::hls
