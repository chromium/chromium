// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_group.h"

#include <optional>
#include <variant>

#include "base/strings/string_number_conversions.h"
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
    const GURL& playlist_uri,
    uint64_t rendition_unique_id) {
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
  if (renditions_map_.contains(MediaTrack::Id{name})) {
    if (HLSQuirks::DeduplicateRenditionNamesInGroup()) {
      name += base::NumberToString(renditions_map_.size());
    } else {
      return ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames;
    }
  }

  std::optional<std::string> language;
  if (tag.language.has_value()) {
    language = std::string(tag.language->Str());
  }

  std::optional<std::string> associated_language;
  if (tag.associated_language.has_value()) {
    associated_language = std::string(tag.associated_language->Str());
  }

  // TODO(crbug.com/371024058): We might want to try figuring out Kind
  // values, but it would have to be post-hoc with respect to the entire
  // rendition group being parsed. We could also figure out a better label
  // value, since that's what gets shown to the used in the track selection UX.
  std::optional<MediaTrack> track;
  switch (tag.type) {
    case MediaType::kAudio: {
      track = MediaTrack::CreateAudioTrack(
          /*id = */ name,
          /*kind =*/MediaTrack::AudioKind::kMain,
          /*label = */ name,
          /*language = */ language.value_or(""),
          /*enabled = */ false,
          /*stream_id =*/rendition_unique_id,
          /*exclusive =*/true);
      break;
    }
    case MediaType::kVideo: {
      track = MediaTrack::CreateVideoTrack(
          /*id = */ name,
          /*kind =*/MediaTrack::VideoKind::kMain,
          /*label = */ name,
          /*language = */ language.value_or(""),
          /*enabled = */ false,
          /*stream_id =*/rendition_unique_id);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  auto& rendition = renditions_.emplace_back(
      base::PassKey<RenditionGroup>(),
      Rendition::CtorArgs{
          .uri = std::move(uri),
          .name = std::move(name),
          .language = std::move(language),
          .associated_language = std::move(associated_language),
          .stable_rendition_id = std::move(tag.stable_rendition_id),
          .channels = std::move(tag.channels),
          .autoselect = std::move(tag.autoselect),
      });

  tracks_.push_back(*track);
  renditions_map_.emplace(track->track_id(),
                          std::make_tuple(*track, &rendition));

  if (tag.is_default) {
    if (!default_rendition_.has_value()) {
      default_rendition_ = std::make_tuple(*track, &rendition);
    } else if (!HLSQuirks::AllowMultipleDefaultRenditionsInGroup()) {
      return ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames;
    }
  }

  return std::monostate();
}

RenditionGroup::RenditionTrack RenditionGroup::MakeImplicitRendition(
    base::PassKey<MultivariantPlaylist>,
    const GURL& default_rendition_uri,
    uint64_t rendition_unique_id) {
  auto& rendition =
      renditions_.emplace_back(base::PassKey<RenditionGroup>(),
                               Rendition::CtorArgs{
                                   .uri = std::move(default_rendition_uri),
                                   .name = "",
                                   .language = std::nullopt,
                                   .associated_language = std::nullopt,
                                   .stable_rendition_id = std::nullopt,
                                   .channels = std::nullopt,
                                   .autoselect = true,
                               });
  MediaTrack track = MediaTrack::CreateVideoTrack(
      /*id = */ rendition.GetName(),
      /*kind =*/MediaTrack::VideoKind::kMain,
      /*label = */ rendition.GetName(),
      /*language = */ "",
      /*enabled = */ true,
      /*stream_id =*/rendition_unique_id);
  return std::make_tuple(track, &rendition);
}

const std::optional<RenditionGroup::RenditionTrack> RenditionGroup::MostSimilar(
    const std::optional<RenditionTrack>& to) const {
#define CHECK_RENDITIONS(expr)                  \
  do {                                          \
    for (const auto& entry : renditions_map_) { \
      if (expr(std::get<1>(entry.second))) {    \
        return entry.second;                    \
      }                                         \
    }                                           \
  } while (0)

  if (to.has_value()) {
    // Find an exact match for the track, and use if if it exists.
    const auto& [track, rendition] = *to;
    auto lookup = renditions_map_.find(track.track_id());
    if (lookup != renditions_map_.end()) {
      if (std::get<0>(lookup->second).stream_id() == track.stream_id()) {
        return lookup->second;
      }
    }

    // If the URI is an exact match, use this one.
    CHECK_RENDITIONS([&](const Rendition* entry) {
      return entry->GetUri() == rendition->GetUri();
    });

    // Prefer to match language next.
    CHECK_RENDITIONS([&](const Rendition* entry) {
      return entry->GetLanguage() == rendition->GetLanguage();
    });
  }

  // We didn't find any URI or language matches, so fall back to default.
  if (default_rendition_.has_value()) {
    return *default_rendition_;
  }

  // Find anything with AUTOSELECT=YES
  CHECK_RENDITIONS([&](const Rendition* e) { return e->MayAutoSelect(); });

#undef CHECK_RENDITIONS

  return std::nullopt;
}

const std::optional<RenditionGroup::RenditionTrack>
RenditionGroup::GetRenditionById(const MediaTrack::Id& id) const {
  auto lookup = renditions_map_.find(id);
  if (lookup == renditions_map_.end()) {
    return std::nullopt;
  }
  return lookup->second;
}

}  // namespace media::hls
