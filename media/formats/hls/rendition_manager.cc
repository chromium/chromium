// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_manager.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/multivariant_playlist.h"
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

std::string GetVariantDisplayString(
    RenditionManager::VariantID id,
    const std::vector<VariantStream::FormatComponent>& components,
    const VariantStream* variant) {
  return variant->Format(components, id.value());
}

std::string GetAudioRenditionDisplayString(const AudioRendition* rendition) {
  // TODO(crbug.com/40057824): Consider displaying characteristics / channels /
  // language and other things rather than just the name.
  return rendition->GetName();
}

}  // namespace

RenditionManager::UpdatedSelections::~UpdatedSelections() = default;
RenditionManager::UpdatedSelections::UpdatedSelections() = default;
RenditionManager::UpdatedSelections::UpdatedSelections(
    const UpdatedSelections&) = default;

RenditionManager::VariantMetadata::~VariantMetadata() = default;
RenditionManager::VariantMetadata::VariantMetadata(const VariantMetadata&) =
    default;
RenditionManager::VariantMetadata::VariantMetadata(
    const VariantStream* stream,
    const AudioRenditionGroup* group)
    : track_id(""), stream(stream), audio_rendition_group(group) {}

RenditionManager::RenditionMetadata::~RenditionMetadata() = default;
RenditionManager::RenditionMetadata::RenditionMetadata(
    const RenditionMetadata&) = default;
RenditionManager::RenditionMetadata::RenditionMetadata(
    const AudioRendition* rendition)
    : track_id(""), rendition(rendition) {}

RenditionManager::~RenditionManager() = default;
RenditionManager::RenditionManager(scoped_refptr<MultivariantPlaylist> playlist,
                                   SelectedCB on_variant_selected,
                                   IsTypeSupportedCallback is_type_supported_cb)
    : playlist_(std::move(playlist)),
      on_variant_selected_(std::move(on_variant_selected)) {
  InitializeVariantMaps(std::move(is_type_supported_cb));
}

std::vector<MediaTrack> RenditionManager::GetSelectableVariants() const {
  std::vector<MediaTrack> result;
  for (const auto& [_, metadata] : selectable_variants_) {
    result.push_back(track_map_.at(metadata.track_id));
  }
  return result;
}

std::vector<MediaTrack> RenditionManager::GetSelectableRenditions() const {
  std::vector<MediaTrack> result;
  if (!selected_variant_.has_value()) {
    // We can't select an audio rendition if the variant hasn't been selected
    return result;
  }
  const auto& variant_metadata = selectable_variants_.at(*selected_variant_);
  for (auto id : variant_metadata.audio_renditions) {
    auto rendition_metadata = selectable_renditions_.at(id);
    result.push_back(track_map_.at(rendition_metadata.track_id));
  }
  return result;
}

void RenditionManager::Reselect(SelectedCallonce callback) {
  auto selections = GetUpdatedSelectionIds();
  if (!selections.variant.has_value() &&
      !selections.audio_rendition.has_value()) {
    std::move(callback).Run(nullptr, nullptr);
    return;
  }

  bool variant_change = false;
  bool rendition_change = false;
  if (selections.variant != selected_variant_) {
    variant_change = true;
    selected_variant_ = selections.variant;
  }
  if (selections.audio_rendition != selected_audio_rendition_) {
    rendition_change = true;
    selected_audio_rendition_ = selections.audio_rendition;
  }

  if (!(variant_change || rendition_change)) {
    // No changes, so do not run the callback.
    return;
  }

  CHECK(selections.variant.has_value());
  const VariantStream* selected_variant =
      selectable_variants_.at(*selected_variant_).stream;
  const AudioRendition* audio_override = nullptr;
  if (selected_audio_rendition_.has_value()) {
    audio_override =
        selectable_renditions_.at(selected_audio_rendition_.value()).rendition;
    if (!audio_override->GetUri().has_value()) {
      // An audio rendition with no uri just plays the content from the
      // selected variant. See section 4.4.6.2.1 of the HLS spec for details.
      // The URI attribute is OPTIONAL unless the TYPE is CLOSED-CAPTIONS, in
      // which case the URI attribute must not be present.
      audio_override = nullptr;
    }
  }

  if (variant_change || (rendition_change && audio_override)) {
    std::move(callback).Run(selected_variant, audio_override);
  }
}

void RenditionManager::SetPreferredAudioRendition(
    std::optional<MediaTrack::Id> rendition_id) {
  if (rendition_id.has_value()) {
    auto track = track_map_.at(rendition_id);
    CHECK(track.type() == MediaTrack::Type::kAudio);
    preferred_audio_rendition_ = RenditionID(track.stream_id());
  } else {
    preferred_audio_rendition_ = std::nullopt;
  }
  Reselect(
      base::BindOnce(on_variant_selected_, AdaptationReason::kUserSelection));
}

void RenditionManager::SetPreferredVariant(
    std::optional<MediaTrack::Id> variant_id) {
  if (variant_id.has_value()) {
    auto track = track_map_.at(variant_id);
    CHECK(track.type() == MediaTrack::Type::kVideo);
    preferred_variant_ = VariantID(track.stream_id());
  } else {
    preferred_variant_ = std::nullopt;
  }
  Reselect(
      base::BindOnce(on_variant_selected_, AdaptationReason::kUserSelection));
}

void RenditionManager::UpdatePlayerResolution(const gfx::Size& resolution) {
  player_resolution_ = resolution;
  Reselect(base::BindOnce(on_variant_selected_,
                          AdaptationReason::kResolutionChange));
}

void RenditionManager::UpdateNetworkSpeed(uint64_t network_bps) {
  AdaptationReason reason = network_bps_ > network_bps
                                ? AdaptationReason::kNetworkDowngrade
                                : AdaptationReason::kNetworkUpgrade;
  network_bps_ = network_bps;
  Reselect(base::BindOnce(on_variant_selected_, reason));
}

bool RenditionManager::HasAnyVariants() const {
  return !selectable_variants_.empty();
}

void RenditionManager::InitializeVariantMaps(
    IsTypeSupportedCallback is_type_supported_cb) {
  bool was_audio_only = false;
  bool has_video = false;
  std::vector<VariantMetadata> variant_ordering;

  // From the spec:
  //   The EXT-X-STREAM-INF tag specifies a Variant Stream, which is a set of
  //   Renditions that can be combined to play the presentation.
  // This player does _not_ support alternative video renditions for variants
  // which only have audio content. If we find any variants with video after
  // finding any audio-only variants, drop all the audio-specific variants and
  // renditions. If we find any audio-only variants after a video variant, do
  // not consider it.
  // TODO(crbug.com/40057824): Is this correct? The spec does not say anything
  // about playlists which have audio-only variants next to variants with video.
  // It might be used in the wild to play only video if the network is truly in
  // bad shape, but it's not clear. I've not run into any playlists in the wild
  // which have this.
  for (const VariantStream& variant : playlist_->GetVariants()) {
    switch (VariantTypeSupported(is_type_supported_cb, variant)) {
      case CodecSupportType::kSupportedAudioVideo:
      case CodecSupportType::kSupportedVideoOnly: {
        has_video = true;
        if (was_audio_only) {
          variant_ordering.clear();
          selectable_variants_.clear();
          selectable_renditions_.clear();
          was_audio_only = false;
        }
        break;
      }
      case CodecSupportType::kSupportedAudioOnly: {
        if (has_video) {
          // Don't add this variant.
          continue;
        }
        was_audio_only = true;
        break;
      }
      case CodecSupportType::kUnsupported: {
        continue;
      }
    }
    VariantMetadata stats{&variant, variant.GetAudioRenditionGroup().get()};
    if (auto group = variant.GetAudioRenditionGroup()) {
      // If there is an audio rendition group associated, then get all of its
      // renditions, ID them, and track them in the selectable variant.
      for (auto& rendition : group->GetRenditions()) {
        auto rendition_id = LookupRendition(&rendition);
        if (!rendition_id.has_value()) {
          rendition_id = rendition_id_gen_.GenerateNextId();
          selectable_renditions_.try_emplace(*rendition_id,
                                             RenditionMetadata(&rendition));
        }
        stats.audio_renditions.insert(*rendition_id);
      }
    }
    variant_ordering.push_back(std::move(stats));
  }

  constexpr auto compare = [](const VariantMetadata& lhs,
                              const VariantMetadata& rhs) {
    // First compare by bandwidth
    if (lhs.stream->GetBandwidth() != rhs.stream->GetBandwidth()) {
      return lhs.stream->GetBandwidth() < rhs.stream->GetBandwidth();
    }

    // Then compare by the HLS SCORE property, if available.
    if (lhs.stream->GetScore().has_value() &&
        rhs.stream->GetScore().has_value()) {
      return *lhs.stream->GetScore() < *rhs.stream->GetScore();
    }

    // Prefer the side which has a score, otherwise, consider LHS better.
    return rhs.stream->GetScore().has_value();
  };

  // All variants are now added, and should be sorted.
  base::ranges::sort(variant_ordering, compare);
  for (const VariantMetadata& stats : std::move(variant_ordering)) {
    selectable_variants_.try_emplace(variant_id_gen_.GenerateNextId(), stats);
  }

  // Use the narrowed down variant set to determine the smallest set of data
  // that can be used to uniquely identify a variant.
  auto format_components = DetermineVariantStreamFormatting();

  for (auto& [variant_id, metadata] : selectable_variants_) {
    auto track = MediaTrack::CreateVideoTrack(
        base::StringPrintf("Video.%d", variant_id.value()),
        MediaTrack::VideoKind::kMain,
        GetVariantDisplayString(variant_id, format_components, metadata.stream),
        /*language=*/"",
        /*enabled=*/false, variant_id.value());
    metadata.track_id = track.track_id();
    track_map_.try_emplace(metadata.track_id, std::move(track));
  }

  for (auto& [rendition_id, metadata] : selectable_renditions_) {
    auto track = MediaTrack::CreateAudioTrack(
        base::StringPrintf("Audio.%d", rendition_id.value()),
        MediaTrack::AudioKind::kMain,
        GetAudioRenditionDisplayString(metadata.rendition),
        metadata.rendition->GetLanguage().value_or(""),
        selected_audio_rendition_ == rendition_id, rendition_id.value(), true);
    metadata.track_id = track.track_id();
    track_map_.try_emplace(metadata.track_id, std::move(track));
  }
}

RenditionManager::UpdatedSelections RenditionManager::GetUpdatedSelectionIds() {
  UpdatedSelections selections;
  selections.variant = preferred_variant_;
  selections.audio_rendition = preferred_audio_rendition_;

  if (!selections.variant.has_value()) {
    // If the user did not specify a video variant, select the "best" one
    // ourselves.
    selections.variant = SelectBestVariant();
  }

  if (!selections.variant.has_value()) {
    // Only when there are no valid variants to play can selections.variant be
    // nullopt at this point. If that is the case, then we should select nothing
    // return both nullopts, and let whoever is responsible for loading the
    // content report an error.
    return selections;
  }

  // The user may have selected a preferece for rendition that was present in a
  // different variant - for example the user may have been playing a stereo
  // rendition for some variant, but then decided to change to a variant which
  // only has audio from a 5.1 surround stream. In this case, we want to select
  // a rendition that is most similar to the user provided one.
  selections.audio_rendition =
      SelectBestRendition(*selections.variant, selections.audio_rendition);

  return selections;
}

std::optional<RenditionManager::VariantID>
RenditionManager::SelectBestVariant() {
  std::optional<VariantID> best = std::nullopt;
  if (selectable_variants_.size()) {
    // If there is at least one thing in the list, then consider the lowest
    // quality entry to be selectable, even if poor performance would otherwise
    // preclude it from selection.
    best = std::get<0>(*selectable_variants_.begin());
  }

  for (auto& [variant_id, stats] : selectable_variants_) {
    const auto& variant_resolution = stats.stream->GetResolution();
    if (variant_resolution.has_value() &&
        variant_resolution->Area() > player_resolution_.Area64()) {
      // This resolution is too large, so return the previous one.
      return best;
    }

    const auto& variant_bps = stats.stream->GetBandwidth();
    if (network_bps_ < variant_bps) {
      // The network is likely not able to keep up with the download rate, so
      // return the previous one.
      return best;
    }

    // This variant is selectable, but keep checking for better ones.
    best = variant_id;
  }

  return best;
}

std::optional<RenditionManager::RenditionID>
RenditionManager::SelectRenditionBasedOnLanguage(
    const VariantMetadata& variant,
    std::optional<std::string> language,
    bool only_autoselect) {
  // Check to see if the default rendition exists and matches the language, if
  // the language is specified. If language is specified but the default does
  // not specify a language at all, consider it a match.
  if (auto* def = variant.audio_rendition_group->GetDefaultRendition()) {
    // The rendition is guaranteed to be present.
    RenditionID id = LookupRendition(def).value();
    if (!language.has_value()) {
      return id;
    }

    auto default_lang = def->GetLanguage();
    if (!default_lang.has_value() || *default_lang == *language) {
      return id;
    }
  }

  // Check the remaining renditions - on this pass, they must be auto-selectable
  // and match the language if it is specified. Return the first match.
  for (const auto id : variant.audio_renditions) {
    if (only_autoselect &&
        !selectable_renditions_.at(id).rendition->MayAutoSelect()) {
      continue;
    }
    const auto& rendition_lang =
        selectable_renditions_.at(id).rendition->GetLanguage();
    if (!language.has_value() || rendition_lang == language) {
      return id;
    }
  }

  if (auto* def = variant.audio_rendition_group->GetDefaultRendition()) {
    // Nothing acceptable matched our language, so select the default, if it
    // exists. The default rendition is guaranteed to exist in the map.
    return LookupRendition(def).value();
  }

  // Select the first remotely acceptable rendition.
  for (const auto id : variant.audio_renditions) {
    if (only_autoselect &&
        !selectable_renditions_.at(id).rendition->MayAutoSelect()) {
      continue;
    }
    return id;
  }

  return std::nullopt;
}

std::optional<RenditionManager::RenditionID>
RenditionManager::SelectBestRendition(
    VariantID variant_id,
    std::optional<RenditionID> maybe_rendition) {
  const auto& variant = selectable_variants_.at(variant_id);

  // If there are no renditions attached to this variant, then select nothing.
  if (!variant.audio_renditions.size() || !variant.audio_rendition_group) {
    return std::nullopt;
  }

  if (!maybe_rendition.has_value()) {
    // The user did not select anything, so we first try to get a default from
    // the group.
    if (auto* def = variant.audio_rendition_group->GetDefaultRendition()) {
      std::optional<RenditionID> id = LookupRendition(def);
      if (id.has_value()) {
        return id.value();
      }
    }

    // Because there is no selected rendition, there is no language default
    // to consider. This should change if we can get a system default language.
    // Also, we should only be considering auto-selectable renditions.
    return SelectRenditionBasedOnLanguage(variant, std::nullopt, true);
  }

  // The user did select a rendition, but it's possible that a user could, say
  // select a stereo variant, then a german rendition, then a 5.1 surround
  // variant. The german rendition is likely to be different in this new variant
  // but we probably want to match it as closely as possible.
  if (variant.audio_renditions.find(*maybe_rendition) !=
      variant.audio_renditions.end()) {
    // The user's rendition is still here, use it.
    return maybe_rendition;
  }

  auto rendition_metadata = selectable_renditions_.at(*maybe_rendition);

  // Use the language from the user's selected rendition (might be nullopt),
  // and also don't consider only auto-selectable things, as the user has made
  // a selection.
  return SelectRenditionBasedOnLanguage(
      variant, rendition_metadata.rendition->GetLanguage(), false);
}

std::optional<RenditionManager::RenditionID> RenditionManager::LookupRendition(
    const AudioRendition* rendition) {
  for (const auto& [id, metadata] : selectable_renditions_) {
    if (metadata.rendition == rendition) {
      return id;
    }
  }
  return std::nullopt;
}

std::vector<VariantStream::FormatComponent>
RenditionManager::DetermineVariantStreamFormatting() const {
  // We have to find some set of properties that differentiates all of the
  // supported variants. The most common differentiator is going to be
  // video resolution. Resolution is always included in the format unless some
  // of the variants are missing it or all variants have the same resolution.
  // Framerate is used as a secondary differentiator to resolution, and is only
  // used when there are two or more variants of the same resolution that have
  // differing frames rates. Bandwidth is used as a tertiary differentiator to
  // resolution and framerate.
  // If we're still in a scenario where resolution, framerate, and bandwidth are
  // all the same, we have to decide to fall back to either codecs, score, uri,
  // or index. For now just fall back to stream index, and include resolution if
  // there is more than one total size available.
  base::flat_set<types::DecimalInteger> resolutions;
  base::flat_set<types::DecimalInteger> rates;
  base::flat_set<types::DecimalInteger> bandwidths;

  bool missing_resolution = false;
  bool missing_frame_rate = false;

  for (const auto& [variant_id, stats] : selectable_variants_) {
    auto resolution = stats.stream->GetResolution();
    auto frame_rate = stats.stream->GetFrameRate();
    auto bandwidth = stats.stream->GetBandwidth();

    if (resolution.has_value()) {
      resolutions.insert(resolution.value().Szudzik());
    } else {
      missing_resolution = true;
    }

    if (frame_rate.has_value()) {
      // FrameRate x Resolution is a bit tricky. We can't just consider one or
      // the other, because {360p, 720p} x {24fps, 60fps} would have four
      // variants, but only two resolutions or two frame rates. This isn't an
      // issue for bandwidth because it isn't an independent property like these
      // two are. To account for the fact that frame rate is only a secondary
      // differentiator, we actually hash it with resolution for a better
      // signal.
      if (frame_rate.value() > 2048) {
        // We don't support this high of a frame rate anyway! This data is
        // probably invalid, so just fall back to stream index.
        return {VariantStream::FormatComponent::kIndex};
      }
      auto resolution_and_rate = frame_rate.value();
      if (resolution.has_value()) {
        resolution_and_rate += resolution.value().Szudzik() << 11;
      }

      rates.insert(resolution_and_rate);
    } else {
      missing_frame_rate = true;
    }

    bandwidths.insert(bandwidth);
  }

  if (resolutions.size() == selectable_variants_.size()) {
    // There are no duplicates of resolution, and every variant provides one.
    return {VariantStream::FormatComponent::kResolution};
  }

  if (rates.size() == selectable_variants_.size()) {
    // The frame rates are a pure differentiator for variant, but we still
    // want to include resolution as well, assuming each variant has one and
    // they are not all the same.
    if (missing_resolution || resolutions.size() == 1) {
      return {VariantStream::FormatComponent::kFrameRate};
    }
    return {VariantStream::FormatComponent::kResolution,
            VariantStream::FormatComponent::kFrameRate};
  }

  if (bandwidths.size() == selectable_variants_.size()) {
    if (missing_resolution || resolutions.size() == 1) {
      // Don't include resolution. Maybe frame rate?
      if (missing_frame_rate || rates.size() == 1) {
        return {VariantStream::FormatComponent::kBandwidth};
      }
      return {VariantStream::FormatComponent::kBandwidth,
              VariantStream::FormatComponent::kFrameRate};
    }
    if (missing_frame_rate || rates.size() == 1) {
      // Don't include frame rate, but resolution is ok.
      return {VariantStream::FormatComponent::kBandwidth,
              VariantStream::FormatComponent::kResolution};
    }
    return {VariantStream::FormatComponent::kBandwidth,
            VariantStream::FormatComponent::kResolution,
            VariantStream::FormatComponent::kFrameRate};
  }

  return {VariantStream::FormatComponent::kIndex};
}

}  // namespace media::hls
