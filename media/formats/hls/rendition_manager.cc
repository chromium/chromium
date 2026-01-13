// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_manager.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/formats/hls/abr_algorithm.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/rendition.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"

namespace media::hls {

namespace {

RenditionManager::CodecSupportType VariantTypeSupported(
    RenditionManager::IsTypeSupportedCallback is_type_supported_cb,
    const VariantStream& variant) {
  // Check if the codecs reported by this variant can be played at all. If
  // this variant does not report its codecs, we'll assume its supported until
  // proven otherwise.
  auto codecs = variant.GetCodecs();
  if (!codecs) {
    return RenditionManager::CodecSupportType::kSupportedAudioVideo;
  }

  RenditionManager::CodecSupportType mp4 =
      is_type_supported_cb.Run("video/mp4", *codecs);
  if (mp4 != RenditionManager::CodecSupportType::kUnsupported) {
    return mp4;
  }

  RenditionManager::CodecSupportType mp2t =
      is_type_supported_cb.Run("video/mp2t", *codecs);
  return mp2t;
}

bool HasVideoSupport(RenditionManager::CodecSupportType cst) {
  return cst == RenditionManager::CodecSupportType::kSupportedAudioVideo ||
         cst == RenditionManager::CodecSupportType::kSupportedVideoOnly;
}

constexpr RenditionManager::CodecSupportType operator|(
    RenditionManager::CodecSupportType l,
    RenditionManager::CodecSupportType r) {
  return static_cast<RenditionManager::CodecSupportType>(
      static_cast<uint8_t>(l) | static_cast<uint8_t>(r));
}

std::vector<raw_ptr<const VariantStream>> FilterVariants(
    const MultivariantPlaylist* playlist,
    RenditionManager::CodecSupportType* supported,
    RenditionManager::IsTypeSupportedCallback support_cb) {
  std::vector<raw_ptr<const VariantStream>> supported_variants;
  *supported = RenditionManager::CodecSupportType::kUnsupported;

  for (const VariantStream& variant : playlist->GetVariants()) {
    auto determined = VariantTypeSupported(support_cb, variant);
    if (determined == RenditionManager::CodecSupportType::kUnsupported) {
      continue;
    }

    if (*supported == RenditionManager::CodecSupportType::kSupportedAudioOnly &&
        HasVideoSupport(determined)) {
      // All these variants were audio only, drop them.
      supported_variants.clear();
    }

    if (HasVideoSupport(determined) || !HasVideoSupport(*supported)) {
      supported_variants.push_back(&variant);
      *supported = *supported | determined;
    }
  }

  constexpr auto compare = [](raw_ptr<const VariantStream>& lhs,
                              raw_ptr<const VariantStream>& rhs) {
    // First compare by bandwidth
    if (lhs->GetBandwidth() != rhs->GetBandwidth()) {
      return lhs->GetBandwidth() < rhs->GetBandwidth();
    }

    // Then compare by the HLS SCORE property, if available.
    if (lhs->GetScore().has_value() && rhs->GetScore().has_value()) {
      return *lhs->GetScore() < *rhs->GetScore();
    }

    // Prefer the side which has a score, otherwise, consider LHS better.
    return rhs->GetScore().has_value();
  };

  std::ranges::sort(supported_variants, compare);
  return supported_variants;
}

bool IsSameRendition(std::optional<hls::RenditionGroup::RenditionTrack> a,
                     std::optional<hls::RenditionGroup::RenditionTrack> b) {
  if (!a.has_value() && !b.has_value()) {
    return true;
  }
  if (!a.has_value() || !b.has_value()) {
    return false;
  }
  return std::get<0>(*a).stream_id() == std::get<0>(*b).stream_id();
}

}  // namespace

RenditionManager::~RenditionManager() = default;
RenditionManager::RenditionManager(scoped_refptr<MultivariantPlaylist> playlist,
                                   SelectedCB reselect_cb,
                                   IsTypeSupportedCallback is_type_supported_cb)
    : playlist_(std::move(playlist)),
      reselect_cb_(std::move(reselect_cb)),
      abr_algorithm_(std::make_unique<EwmaAbrAlgorithm>()) {
  // Don't even bother considering unsupported types. This is somewhat like
  // calling `isTypeSupported()` in a JS player, except that we return true if
  // the variant has no codecs.
  // TODO(crbug.com/XXXXX): In the case of no codecs listed, we probably want to
  // have a mechanism by which we can test these renditions and asynchronously
  // either enable or disable them.
  selectable_variants_ = FilterVariants(playlist_.get(), &supported_streams_,
                                        std::move(is_type_supported_cb));

  // Based on player metrics (ABR Speed & Resolution)
  const auto* best_variant = SelectBestVariant();

  UpdateAudioRenditions(best_variant);
  UpdateVideoRenditions();
}

const VariantStream* RenditionManager::SelectBestVariant() const {
  if (selectable_variants_.empty()) {
    return nullptr;
  }

  if (preferred_variant_.has_value()) {
    return preferred_variant_.value();
  }

  const VariantStream* best = *selectable_variants_.begin();
  const auto abr_speed = abr_algorithm_->GetABRSpeed();

  // Figuring out what the "best" resolution that can / should be played is
  // somewhat tricky. For example, if a user's monitor is 1920x1080 but the
  // video element isn't fullscreen'd, the actual player space is slightly less
  // due to window borders or the browser UX on the top. A player should be able
  // to select a rendition with a _slightly_ larger resolution than the actual
  // player dimensions as a result - but not too much bigger, because it doesn't
  // make sense to be playing 2560x1440p content on that same 1080p monitor.
  constexpr float kPlayerDimensionScaleFactor = 1.1;
  const types::DecimalInteger max_width =
      player_resolution_.width() * kPlayerDimensionScaleFactor;
  const types::DecimalInteger max_height =
      player_resolution_.height() * kPlayerDimensionScaleFactor;

  for (const VariantStream* option : selectable_variants_) {
    if (option->GetResolution().has_value()) {
      if (player_resolution_.Area64() < option->GetResolution()->Area()) {
        // This variant is too large to even fit in the player area, so don't
        // consider it.
        if (max_height < option->GetResolution()->height &&
            max_width < option->GetResolution()->width) {
          // This video variant is too large to be useful to this player
          // resolution, so don't consider it.
          return best;
        }
      }
    }

    if (abr_speed < option->GetBandwidth()) {
      // This variant is predicted to have a bandwidth requirement greater than
      // the connection speed to the host, so don't consider it.
      return best;
    }

    best = option;
  }

  return best;
}

void RenditionManager::UpdateAudioRenditions(const VariantStream* best) {
  if (!best) {
    selectable_audio_tracks_.clear();
    return;
  }

  if (supported_streams_ == CodecSupportType::kSupportedAudioOnly) {
    // For audio-only content, the set of selectable tracks has to be every
    // unique rendition from each variant. By exposing all audio tracks to the
    // player, at least any JS-based player implementation can parse the set of
    // tracks and build separate selectors for language/bitrate/etc.
    if (!selectable_audio_tracks_.empty()) {
      return;
    }
    base::flat_map<MediaTrack::Id, MediaTrack> tracks;
    for (const auto& variant : selectable_variants_) {
      auto& renditions = variant->GetAudioRenditionGroup();
      for (const auto& track : renditions.GetTracks()) {
        tracks.insert({track.track_id(), track});
        auto rendition = renditions.GetRenditionById(track.track_id());
        CHECK(rendition);
        track_map_.insert({track.track_id(), *rendition});
      }
    }
    for (const auto& pair : tracks) {
      selectable_audio_tracks_.push_back(std::move(pair.second));
    }
    return;
  }

  // For AV content, the audio tracks are subordinate to the video variant
  // selected.
  selectable_audio_tracks_ = {
      best->GetAudioRenditionGroup().GetTracks().begin(),
      best->GetAudioRenditionGroup().GetTracks().end()};
}

void RenditionManager::UpdateVideoRenditions() {
  if (supported_streams_ == CodecSupportType::kSupportedAudioOnly) {
    return;
  }

  for (const auto& variant : selectable_variants_) {
    auto& renditions = variant->GetVideoRenditionGroup();
    for (const auto& track : renditions.GetTracks()) {
      selectable_video_tracks_.push_back(track);
      auto rendition = renditions.GetRenditionById(track.track_id());
      DCHECK(rendition);
      track_map_.insert({track.track_id(), *rendition});
    }
  }
}

void RenditionManager::Reselect(SelectedCallonce cb) {
  const VariantStream* variant = SelectBestVariant();
  if (variant == nullptr) {
    std::move(cb).Run(nullptr, std::nullopt, std::nullopt);
    return;
  }

  if (supported_streams_ == CodecSupportType::kSupportedAudioOnly) {
    if (preferred_audio_rendition_.has_value()) {
      // The user's audio preference is always selected as-is for audio-only
      // content. Only fire the selected callback if the preference is not the
      // same as the active primary rendition.
      active_variant_ = *preferred_associated_variant_;
      if (!IsSameRendition(preferred_audio_rendition_,
                           selected_primary_rendition_)) {
        selected_primary_rendition_ = preferred_audio_rendition_;
        std::move(cb).Run(variant, selected_primary_rendition_, std::nullopt);
      }
      return;
    }

    auto rendition = variant->GetAudioRenditionGroup().MostSimilar(
        selected_primary_rendition_);
    if (!IsSameRendition(rendition, selected_primary_rendition_)) {
      active_variant_ = variant;
      selected_primary_rendition_ = rendition;
      std::move(cb).Run(variant, selected_primary_rendition_, std::nullopt);
    }

    return;
  }

  std::optional<RenditionGroup::RenditionTrack> video_rendition = std::nullopt;
  std::optional<RenditionGroup::RenditionTrack> audio_rendition = std::nullopt;
  const VariantStream* new_variant = nullptr;
  bool variant_changed = false;

  if (preferred_video_rendition_.has_value()) {
    new_variant = preferred_associated_variant_.value();
    video_rendition = preferred_video_rendition_;
  } else {
    new_variant = variant;
    video_rendition = variant->GetVideoRenditionGroup().MostSimilar(
        selected_primary_rendition_);
  }

  if (active_variant_ != new_variant) {
    variant_changed = true;
    active_variant_ = new_variant;
    UpdateAudioRenditions(new_variant);
  }

  auto& audio_group = new_variant->GetAudioRenditionGroup();

  // If there are no shared tracks, then the rendition is always implicit,
  // which means it's included in the primary rendition URI.
  if (audio_group.HasSharedTracks()) {
    auto ideal_audio_rendition = preferred_audio_rendition_;
    if (!ideal_audio_rendition) {
      ideal_audio_rendition = selected_extra_rendition_;
    }
    audio_rendition = audio_group.MostSimilar(ideal_audio_rendition);
  }

  // This might be an audio rendition which is included in the primary video
  // rendition if we have selected it.
  if (audio_rendition.has_value() &&
      !std::get<1>(*audio_rendition)->GetUri().has_value()) {
    audio_rendition = std::nullopt;
  }

  if (variant_changed ||
      !IsSameRendition(video_rendition, selected_primary_rendition_) ||
      !IsSameRendition(audio_rendition, selected_extra_rendition_)) {
    selected_primary_rendition_ = video_rendition;
    selected_extra_rendition_ = audio_rendition;
    std::move(cb).Run(active_variant_, video_rendition, audio_rendition);
  }
}

void RenditionManager::SetPreferredAudioRendition(
    std::optional<MediaTrack::Id> track_id) {
  if (!track_id.has_value()) {
    // The user has elected to deselect all audio tracks - The chunk demuxer
    // we wrap to do content demuxing has already disabled it's DemuxerStream
    // objects, so no audio will play.
    // While we can unset the preferences object here, we don't want to unset
    // the `selected_extra_rendition_` field, since when re-enabling audio, we
    // don't want to trigger a rendition update if the same rendition is still
    // playing.
    preferred_audio_rendition_ = std::nullopt;
    return;
  }

  if (supported_streams_ == CodecSupportType::kSupportedAudioOnly) {
    auto lookup = track_map_.find(*track_id);
    if (lookup == track_map_.end()) {
      return;
    }

    for (const auto& variant : selectable_variants_) {
      for (const auto& track : variant->GetAudioRenditionGroup().GetTracks()) {
        if (track.track_id() == track_id) {
          preferred_associated_variant_ = variant;
          break;
        }
      }
    }

    DCHECK(preferred_associated_variant_);
    preferred_audio_rendition_ = lookup->second;
    Reselect(base::BindOnce(reselect_cb_, AdaptationReason::kUserSelection));
    return;
  }

  if (!active_variant_) {
    // Track ID's are only unique across a RenditionGroup - so if we don't have
    // access to the current rendition group, we can't actually have any
    // preferred renditions.
    return;
  }

  preferred_audio_rendition_ =
      active_variant_->GetAudioRenditionGroup().GetRenditionById(*track_id);
  Reselect(base::BindOnce(reselect_cb_, AdaptationReason::kUserSelection));
}

void RenditionManager::SetPreferredVideoRendition(
    std::optional<MediaTrack::Id> track_id) {
  CHECK(supported_streams_ != CodecSupportType::kSupportedAudioOnly);

  if (!track_id.has_value()) {
    preferred_video_rendition_ = std::nullopt;
    preferred_associated_variant_ = std::nullopt;
    return;
  }

  auto lookup = track_map_.find(*track_id);
  if (lookup == track_map_.end()) {
    return;
  }

  preferred_video_rendition_ = lookup->second;

  for (const auto& variant : selectable_variants_) {
    for (const auto& track : variant->GetVideoRenditionGroup().GetTracks()) {
      if (track.track_id() == track_id) {
        preferred_associated_variant_ = variant;
        break;
      }
    }
  }

  CHECK(preferred_associated_variant_.has_value());
  Reselect(base::BindOnce(reselect_cb_, AdaptationReason::kUserSelection));
}

void RenditionManager::UpdatePlayerResolution(const gfx::Size& resolution) {
  player_resolution_ = resolution;
  Reselect(base::BindOnce(reselect_cb_, AdaptationReason::kResolutionChange));
}

void RenditionManager::UpdateNetworkSpeed(uint64_t network_bps) {
  const auto old_speed = abr_algorithm_->GetABRSpeed();
  abr_algorithm_->UpdateNetworkSpeed(network_bps);
  const auto new_speed = abr_algorithm_->GetABRSpeed();

  AdaptationReason reason = old_speed > new_speed
                                ? AdaptationReason::kNetworkDowngrade
                                : AdaptationReason::kNetworkUpgrade;
  Reselect(base::BindOnce(reselect_cb_, reason));
}

void RenditionManager::SetAbrAlgorithmForTesting(
    std::unique_ptr<ABRAlgorithm> abr_algorithm) {
  abr_algorithm_ = std::move(abr_algorithm);
}

}  // namespace media::hls
