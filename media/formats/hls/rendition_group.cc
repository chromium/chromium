// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_group.h"

#include <optional>
#include <variant>

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/pass_key.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/quirks.h"
#include "media/formats/hls/rendition.h"
#include "media/formats/hls/tags.h"

namespace media::hls {

namespace {

MediaTrack CreateTrackForRendition(const Rendition& rendition,
                                   MediaType type,
                                   RenditionGroup::RenditionTrackId unique_id) {
  switch (type) {
    case MediaType::kAudio: {
      return MediaTrack::CreateAudioTrack(
          /*id = */ rendition.GetName(),
          /*kind =*/MediaTrack::AudioKind::kMain,
          /*label = */ rendition.GetName(),
          /*language = */ rendition.GetLanguage().value_or(""),
          /*enabled = */ false,
          /*stream_id =*/unique_id.value(),
          /*exclusive =*/true);
    }
    case MediaType::kVideo: {
      return MediaTrack::CreateVideoTrack(
          /*id = */ rendition.GetName(),
          /*kind =*/MediaTrack::VideoKind::kMain,
          /*label = */ rendition.GetName(),
          /*language = */ rendition.GetLanguage().value_or(""),
          /*enabled = */ false,
          /*stream_id =*/unique_id.value());
    }
    default: {
      NOTREACHED();
    }
  }
}

}  // namespace

RenditionGroup::View::~View() = default;
RenditionGroup::View::View(scoped_refptr<RenditionGroup> group,
                           Rendition rendition,
                           MediaTrack track)
    : group_(std::move(group)),
      rendition_(std::move(rendition)),
      track_(std::make_tuple(std::move(track), &rendition_)) {}

void RenditionGroup::View::UpdateImplicitRenditionMediaTrackName(
    std::string name) {
  auto old_track = std::get<0>(track_);
  track_ = std::make_tuple(MediaTrack::CreateVideoTrack(
                               /*id = */ name,
                               /*kind =*/MediaTrack::VideoKind::kMain,
                               /*label = */ name,
                               /*language = */ "",
                               /*enabled = */ old_track.enabled(),
                               /*stream_id =*/old_track.stream_id()),
                           &rendition_);
}

RenditionGroup::RenditionGroup(base::PassKey<MultivariantPlaylist>,
                               std::optional<std::string> id)
    : id_(std::move(id)) {}

RenditionGroup::~RenditionGroup() = default;

ParseStatus::Or<std::monostate> RenditionGroup::AddRendition(
    base::PassKey<MultivariantPlaylist>,
    XMediaTag tag,
    const GURL& playlist_uri,
    std::optional<GURL> resolved_playlist_uri,
    RenditionTrackId unique_id) {
  DCHECK(tag.group_id.Str() == id_);
  DCHECK(playlist_uri.is_valid());

  // INSTREAM-ID MUST NOT be specified for any rendition with a TYPE other than
  // CLOSED-CAPTIONS. We don't support CLOSED-CAPTIONS.
  DCHECK(tag.instream_id == std::nullopt);

  // FORCED MUST NOT be present unless the TYPE is SUBTITLES. We don't support
  // the SUBTITLES type.
  DCHECK(tag.forced == false);

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

  auto& rendition = renditions_.emplace_back(
      base::PassKey<RenditionGroup>(),
      Rendition::CtorArgs{
          .uri = std::move(resolved_playlist_uri),
          .name = std::move(name),
          .language = std::move(language),
          .associated_language = std::move(associated_language),
          .stable_rendition_id = std::move(tag.stable_rendition_id),
          .channels = std::move(tag.channels),
          .autoselect = std::move(tag.autoselect),
      });

  // TODO(crbug.com/371024058): We might want to try figuring out Kind
  // values, but it would have to be post-hoc with respect to the entire
  // rendition group being parsed. We could also figure out a better label
  // value, since that's what gets shown to the used in the track selection UX.
  MediaTrack track = CreateTrackForRendition(rendition, tag.type, unique_id);
  tracks_.push_back(track);
  renditions_map_.emplace(track.track_id(), std::make_tuple(track, &rendition));

  if (tag.is_default) {
    if (!default_rendition_.has_value()) {
      default_rendition_ = std::make_tuple(track, &rendition);
    } else if (!HLSQuirks::AllowMultipleDefaultRenditionsInGroup()) {
      return ParseStatusCode::kRenditionGroupHasDuplicateRenditionNames;
    }
  }

  return std::monostate();
}

std::unique_ptr<RenditionGroup::View> RenditionGroup::MakeImplicitView(
    base::PassKey<MultivariantPlaylist>,
    MediaType type,
    const GURL& default_rendition_uri,
    RenditionTrackId unique_id) {
  Rendition rendition = Rendition{base::PassKey<RenditionGroup>(),
                                  Rendition::CtorArgs{
                                      .uri = std::move(default_rendition_uri),
                                      .name = "Default",
                                      .language = std::nullopt,
                                      .associated_language = std::nullopt,
                                      .stable_rendition_id = std::nullopt,
                                      .channels = std::nullopt,
                                      .autoselect = true,
                                  }};
  auto track = CreateTrackForRendition(rendition, type, unique_id);
  return std::make_unique<View>(base::WrapRefCounted(this),
                                std::move(rendition), std::move(track));
}

const std::optional<RenditionGroup::RenditionTrack>
RenditionGroup::View::MostSimilar(
    const std::optional<RenditionTrack>& to) const {
#define CHECK_RENDITIONS(expr)                          \
  do {                                                  \
    for (const auto& entry : group_->renditions_map_) { \
      if (expr(std::get<1>(entry.second))) {            \
        return entry.second;                            \
      }                                                 \
    }                                                   \
  } while (0)

  if (to.has_value()) {
    // Find an exact match for the track, and use if if it exists.
    const auto& [track, rendition] = *to;
    auto lookup = group_->renditions_map_.find(track.track_id());
    if (lookup != group_->renditions_map_.end()) {
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
  if (group_->default_rendition_.has_value()) {
    return *group_->default_rendition_;
  }

  // Find anything with AUTOSELECT=YES
  CHECK_RENDITIONS([&](const Rendition* e) { return e->MayAutoSelect(); });

#undef CHECK_RENDITIONS

  return GetImplicitRenditionTrack();
}

const std::optional<RenditionGroup::RenditionTrack>
RenditionGroup::View::GetRenditionById(const MediaTrack::Id& id) const {
  if (!group_->default_rendition_.has_value() &&
      id == std::get<0>(track_).track_id()) {
    return GetImplicitRenditionTrack();
  }
  auto lookup = group_->renditions_map_.find(id);
  if (lookup == group_->renditions_map_.end()) {
    return std::nullopt;
  }
  return lookup->second;
}

}  // namespace media::hls
