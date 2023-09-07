// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/rendition_selector.h"

#include <string>

#include "base/ranges/algorithm.h"
#include "media/formats/hls/audio_rendition.h"
#include "media/formats/hls/multivariant_playlist.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media::hls {

namespace {

RenditionSelector::CodecSupportType VariantTypeSupported(
    RenditionSelector::IsTypeSupportedCallback is_type_supported_cb,
    const VariantStream& variant) {
  // Check if the codecs reported by this variant can be played at all. If
  // this variant does not report its codecs, we'll assume its supported until
  // proven otherwise.
  auto codecs = variant.GetCodecs();
  if (!codecs) {
    return RenditionSelector::CodecSupportType::kSupportedAudioVideo;
  }

  RenditionSelector::CodecSupportType mp4 =
      is_type_supported_cb.Run("video/mp4", *codecs);
  if (mp4 != RenditionSelector::CodecSupportType::kUnsupported) {
    return mp4;
  }

  RenditionSelector::CodecSupportType mp2t =
      is_type_supported_cb.Run("video/mp2t", *codecs);
  return mp2t;
}

// |OptimalFilter| will apply an |IsAcceptable (T)->Bool| filter function over
// a set of |options| and filter them based on on the result. However, in the
// case where nothing in |options| is acceptable, it will use a
// |ComputeFallbackCandidate (T, T) -> T| function to ensure that the resulting
// list always has at least one item in it.
template <typename IsAcceptable, typename ComputeFallbackCandidate>
std::vector<const VariantStream*> OptimalFilter(
    const std::vector<const VariantStream*>& options,
    IsAcceptable filter_functor,
    ComputeFallbackCandidate compare_functor) {
  std::vector<const VariantStream*> acceptable;
  const VariantStream* best_fallback_candidate = nullptr;

  for (const VariantStream* test : options) {
    if (filter_functor(test)) {
      acceptable.push_back(test);
    }
    if (acceptable.empty()) {
      if (best_fallback_candidate == nullptr) {
        best_fallback_candidate = test;
      } else {
        best_fallback_candidate =
            compare_functor(best_fallback_candidate, test);
      }
    }
  }

  if (acceptable.empty() && best_fallback_candidate != nullptr) {
    acceptable.push_back(best_fallback_candidate);
  }

  return acceptable;
}

// Use the |OptimalFilter| function above to find all Variants under a max bit
// rate, or, if everything is over the max bitrate, find the lowest one.
std::vector<const VariantStream*> GetPreferredVariantsByBitrate(
    absl::optional<types::DecimalInteger> max_bitrate,
    std::vector<const VariantStream*> inputs) {
  if (!max_bitrate.has_value()) {
    return inputs;
  }

  // TODO: Prefer to use average bandwidth, if the streams have that calculated.
  types::DecimalInteger bitrate = *max_bitrate;
  return OptimalFilter(
      inputs,
      /*filter_functor=*/
      [bitrate](const VariantStream* test) {
        return test->GetBandwidth() <= bitrate;
      },
      /*compare_functor=*/
      [](const VariantStream* A, const VariantStream* B) {
        return A->GetBandwidth() > B->GetBandwidth() ? B : A;
      });
}

}  // namespace

RenditionSelector::RenditionSelector(
    scoped_refptr<MultivariantPlaylist> playlist,
    IsTypeSupportedCallback is_type_supported_cb)
    : playlist_(std::move(playlist)) {
  DCHECK(playlist_);
  DCHECK(is_type_supported_cb);

  for (const VariantStream& variant : playlist_->GetVariants()) {
    bool has_video = false;
    bool has_audio = false;

    switch (VariantTypeSupported(is_type_supported_cb, variant)) {
      case CodecSupportType::kUnsupported:
        break;
      case CodecSupportType::kSupportedAudioVideo:
        has_audio = true;
        has_video = true;
        break;
      case CodecSupportType::kSupportedAudioOnly:
        has_audio = true;
        break;
      case CodecSupportType::kSupportedVideoOnly:
        has_video = true;
        break;
    }

    // Don't consider unsupported types!
    if (has_audio || has_video) {
      if (variant.GetAudioRenditionGroup()) {
        has_audio = true;
      }

      if (variant.GetVideoRenditionGroupName().has_value()) {
        has_video = true;
      }

      if (has_video) {
        video_variants_.push_back(&variant);
      }

      if (has_audio) {
        audio_variants_.push_back(&variant);
      }
    }
  }

  // Sort variants by score and bandwidth (descending)
  constexpr auto compare = [](const VariantStream* lhs,
                              const VariantStream* rhs) {
    // First compare by |BANDWIDTH|
    if (lhs->GetBandwidth() != rhs->GetBandwidth()) {
      return lhs->GetBandwidth() > rhs->GetBandwidth();
    }

    // Then compare by SCORE
    if (lhs->GetScore().has_value() && rhs->GetScore().has_value()) {
      return *lhs->GetScore() > *rhs->GetScore();
    }

    // If |lhs| has a score, but |rhs| doesn't, then lhs should be preferred.
    return lhs->GetScore().has_value();
  };

  base::ranges::sort(video_variants_, compare);
  base::ranges::sort(audio_variants_, compare);
}

RenditionSelector::RenditionSelector(const RenditionSelector&) = default;

RenditionSelector::RenditionSelector(RenditionSelector&&) = default;

RenditionSelector& RenditionSelector::operator=(const RenditionSelector&) =
    default;

RenditionSelector& RenditionSelector::operator=(RenditionSelector&&) = default;

RenditionSelector::~RenditionSelector() = default;

RenditionSelector::PreferredVariants RenditionSelector::GetPreferredVariants(
    VideoPlaybackPreferences video_preferences,
    AudioPlaybackPreferences audio_preferences) const {
  // Start by selecting the preferred video variant, which might also be an
  // audio variant. If it's not an audio variant, we have to select the audio
  // variant after. Get the acceptable bitrate variants first, then filter by
  // resolution. From there, we select the highest bitrate one, which should be
  // first, as they are sorted descending by bitrate.
  std::vector<const VariantStream*> acceptable_bitrates =
      GetPreferredVariantsByBitrate(video_preferences.max_smooth_bitrate,
                                    video_variants_);

  // TODO: prefer variants which have the same aspect ratio, even if they are
  // a bit larger, as that will probably look better.
  std::vector<const VariantStream*> acceptable_resolutions;
  if (video_preferences.video_player_resolution.has_value()) {
    gfx::Size resolution = *video_preferences.video_player_resolution;
    acceptable_resolutions = OptimalFilter(
        acceptable_bitrates,
        /*filter_functor=*/
        [resolution](const VariantStream* test) {
          auto stream_res = test->GetResolution();
          if (!stream_res.has_value()) {
            return true;
          }
          if (stream_res->Area() > resolution.Area64()) {
            return false;
          }
          return true;
        },
        /*compare_functor=*/
        [](const VariantStream* A, const VariantStream* B) {
          // If either A or B didn't have a resolution, then there is at least
          // A or B selected already, and this compare_function shouldn't be
          // getting called.
          DCHECK(A->GetResolution().has_value());
          DCHECK(B->GetResolution().has_value());

          // If we have to select something and all the resolutions are too
          // large, select the smallest resolution.
          return A->GetResolution()->Area() > B->GetResolution()->Area() ? B
                                                                         : A;
        });
  } else {
    acceptable_resolutions = acceptable_bitrates;
  }

  PreferredVariants result;

  // This could be audio only, so it's not quite an error yet. Lets grab the
  // optimal audio variant, and try that. If that's also null, then we have no
  // good variant to play, and we should return an error.
  if (acceptable_resolutions.empty()) {
    DCHECK(video_variants_.empty());
    auto acceptable_audio = GetPreferredVariantsByBitrate(
        video_preferences.max_smooth_bitrate, audio_variants_);
    if (acceptable_audio.empty()) {
      return result;
    }
    result.selected_variant = acceptable_audio[0];
    result.audio_override_rendition =
        TryFindAudioOverride(audio_preferences, result.selected_variant);

    // If our selected variant is audio, but we got a rendition that overrides
    // this, then this selected variant won't actually be selected, since it
    // would be overridden by the audio.
    if (result.audio_override_rendition != nullptr) {
      result.audio_override_variant = result.selected_variant;
      result.selected_variant = nullptr;
    }

    return result;
  }

  // For now, since we only select a potential override from the selected
  // variant, the audio override variant is always the same.
  result.selected_variant = acceptable_resolutions[0];
  result.audio_override_variant = acceptable_resolutions[0];

  // If our selected variant is an audio/video stream, we should use its audio
  // group to determine what rendition to pick as an override, if it has an
  // audio group. If we don't suspect it of being an audio stream, then we have
  // to decide what to do - most implementations just end up with silent video.
  // Should we consider finding the best audio variant as well?
  result.audio_override_rendition =
      TryFindAudioOverride(audio_preferences, result.selected_variant);
  return result;
}

const AudioRendition* RenditionSelector::TryFindAudioOverride(
    AudioPlaybackPreferences audio_preferences,
    const VariantStream* variant) const {
  // Note that we're not considering channel count, which should probably be
  // changed at some point.
  CHECK(variant);

  // There are no audio renditions related to this variant anyway.
  if (!variant->GetAudioRenditionGroup()) {
    return nullptr;
  }

  // If there is no preference for language, then we can just use the default.
  if (!audio_preferences.language.has_value()) {
    audio_preferences.language = default_audio_language_;
  }

  // If we have a preference, check all the renditions that are connected to our
  // variant.
  if (audio_preferences.language.has_value()) {
    const auto& renditions = variant->GetAudioRenditionGroup()->GetRenditions();
    for (const auto& rendition : renditions) {
      if (rendition.GetAssociatedLanguage() == *audio_preferences.language) {
        return &rendition;
      }
    }
  }

  // Otherwise, we should just select the one that is marked as DEFAULT (which
  // implies that AUTOSELECT is also true)
  if (variant->GetAudioRenditionGroup()->GetDefaultRendition()) {
    return variant->GetAudioRenditionGroup()->GetDefaultRendition();
  }

  // If there is no default, and we have no preference, then just grab the
  // first thing that was present in the manifest.
  const auto& renditions = variant->GetAudioRenditionGroup()->GetRenditions();
  if (renditions.size()) {
    return &renditions.front();
  }

  // nothing to select!
  return nullptr;
}

}  // namespace media::hls
