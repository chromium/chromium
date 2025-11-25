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

std::vector<raw_ptr<const VariantStream>> FilterVariants(
    const MultivariantPlaylist* playlist,
    bool* is_audio_only,
    RenditionManager::IsTypeSupportedCallback support_cb) {
  std::vector<raw_ptr<const VariantStream>> supported_variants;
  *is_audio_only = true;
  for (const VariantStream& variant : playlist->GetVariants()) {
    switch (VariantTypeSupported(support_cb, variant)) {
      case RenditionManager::CodecSupportType::kSupportedAudioVideo:
      case RenditionManager::CodecSupportType::kSupportedVideoOnly: {
        if (*is_audio_only) {
          supported_variants.clear();
        }
        *is_audio_only = false;
        break;
      }
      case RenditionManager::CodecSupportType::kSupportedAudioOnly: {
        if (!*is_audio_only) {
          continue;
        }
        break;
      }
      case RenditionManager::CodecSupportType::kUnsupported: {
        continue;
      }
    }
    // This variant is supported and possible to select (IE, it is not an
    // audio-only variant in a stream with video-present variants).
    supported_variants.push_back(&variant);
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
  selectable_variants_ = FilterVariants(playlist_.get(), &audio_only_,
                                        std::move(is_type_supported_cb));
  for (const auto& variant : selectable_variants_) {
    selectable_variant_tracks_.push_back(
        std::get<0>(variant->GetImplicitRendition()));
  }
}

const VariantStream* RenditionManager::SelectBestVariant() const {
  // TODO(crbug.com/361853710): This implementation is exclusively based on
  // player parameters, like network speed and player dimensions. Users can't
  // select a preferred variant.
  if (selectable_variants_.empty()) {
    return nullptr;
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

void RenditionManager::Reselect(SelectedCallonce cb) {
  const VariantStream* variant = SelectBestVariant();
  if (variant == nullptr) {
    std::move(cb).Run(nullptr, std::nullopt, std::nullopt);
    return;
  }

  bool variant_changed = false;
  std::optional<RenditionGroup::RenditionTrack> extra_rendition;

  if (active_variant_ != variant) {
    // Either this is initial selection or the active variant is changing due
    // to stream constraints.
    selected_primary_ = variant->GetImplicitRendition();
    active_variant_ = variant;
    variant_changed = true;
  }

  // The extra rendition is always sourced from the audio groups, since nobody
  // really uses video renditions right now. If the whole stream is audio-only,
  // this extra rendition replaces the primary rendition. This selection is
  // based primarily on user preference, but if no preference is set, it
  // attempts to select based on the following order:
  // 1. Similarity to previously selected rendition
  // 2. Group default
  // 3. Declaration order in manifest
  // It's possible that none of the renditions are marked for auto-selection
  // though, so we may get back nothing.
  auto audio_renditions = variant->GetAudioRenditionGroup();
  if (preferred_extra_rendition_.has_value()) {
    extra_rendition = audio_renditions->MostSimilar(preferred_extra_rendition_);
  }
  if (!extra_rendition.has_value()) {
    extra_rendition = audio_renditions->MostSimilar(selected_extra_);
  }
  if (extra_rendition.has_value() &&
      !std::get<1>(extra_rendition.value())->GetUri().has_value()) {
    // An audio rendition with no uri just plays the content from the
    // selected variant. See section 4.4.6.2.1 of the HLS spec for details.
    // The URI attribute is OPTIONAL unless the TYPE is CLOSED-CAPTIONS, in
    // which case the URI attribute must not be present.
    extra_rendition = std::nullopt;
  }

  if (!IsSameRendition(extra_rendition, selected_extra_)) {
    selected_extra_ = extra_rendition;
    if (audio_only_) {
      std::move(cb).Run(variant, extra_rendition, std::nullopt);
      return;
    }

    std::move(cb).Run(variant, selected_primary_, extra_rendition);
    return;
  }

  if (variant_changed) {
    selected_extra_ = extra_rendition;
    std::move(cb).Run(variant, selected_primary_, selected_extra_);
  }
}

void RenditionManager::SetPreferredAudioRendition(
    std::optional<MediaTrack::Id> track_id) {
  if (!active_variant_) {
    // Track ID's are only unique across a RenditionGroup - so if we don't have
    // access to the current rendition group, we can't actually have any
    // preferred renditions.
    return;
  }

  if (!track_id.has_value()) {
    // The user has elected to deselect all audio tracks - The chunk demuxer
    // we wrap to do content demuxing has already disabled it's DemuxerStream
    // objects, so no audio will play.
    // While we can unset the preferences object here, we don't want to unset
    // the `selected_extra_` field, since when re-enabling audio, we don't want
    // to trigger a rendition update if the same rendition is still playing.
    preferred_extra_rendition_ = std::nullopt;
    return;
  }

  preferred_extra_rendition_ =
      active_variant_->GetAudioRenditionGroup()->GetRenditionById(*track_id);
  Reselect(base::BindOnce(reselect_cb_, AdaptationReason::kUserSelection));
}

void RenditionManager::SetPreferredVideoRendition(
    std::optional<MediaTrack::Id> track_id) {
  // Primary (video) rendition selection is not supported.
  NOTREACHED();
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

std::vector<MediaTrack> RenditionManager::GetSelectableAudioRenditions() const {
  if (active_variant_) {
    return active_variant_->GetAudioRenditionGroup()->GetTracks();
  }
  return {};
}

std::vector<MediaTrack> RenditionManager::GetSelectableVideoRenditions() const {
  return selectable_variant_tracks_;
}

}  // namespace media::hls
