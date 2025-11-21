// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_
#define MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/types/id_type.h"
#include "media/base/demuxer.h"
#include "media/base/limits.h"
#include "media/base/media_export.h"
#include "media/formats/hls/abr_algorithm.h"
#include "media/formats/hls/rendition_group.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "ui/gfx/geometry/size.h"

namespace media::hls {

class MultivariantPlaylist;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AdaptationReason {
  kUserSelection = 0,
  kResolutionChange = 1,
  kNetworkUpgrade = 2,
  kNetworkDowngrade = 3,

  kMaxValue = kNetworkDowngrade,
};

// Manages the rendition selection algorithm based on user preference, network
// speed, frame drops, underflow events, and player resolution. Any time one of
// these changes might affect the selected renditions, an update callback is
// fired with the new selections. In the future, this may be extended from the
// present {primary, optional<extra>} rendition set to include subtitles.
class MEDIA_EXPORT RenditionManager {
 public:
  // We want to ask if a codec string is supported, but also if it contains
  // audio, video, or both types of content, allowing us to sort our variants
  // into groups.
  enum class CodecSupportType {
    kUnsupported,
    kSupportedAudioVideo,
    kSupportedAudioOnly,
    kSupportedVideoOnly,
  };

  // Callback used to query whether the given MIME+codec string is supported,
  // and what types of content we can expect.
  // The first argument is the name of the container, ie "video/mp4" or
  // "video/mp2t", and the second argument is a list of codecs to check support
  // for, such as ["mp4a.40.2", "avc1.4d4015"]
  using IsTypeSupportedCallback =
      base::RepeatingCallback<CodecSupportType(std::string_view,
                                               base::span<const std::string>)>;

  // Callbacks for handling rendition/track changes.
  using SelectedCB = base::RepeatingCallback<void(
      AdaptationReason,
      const VariantStream*,
      std::optional<RenditionGroup::RenditionTrack>,
      std::optional<RenditionGroup::RenditionTrack>)>;

  using SelectedCallonce =
      base::OnceCallback<void(const VariantStream*,
                              std::optional<RenditionGroup::RenditionTrack>,
                              std::optional<RenditionGroup::RenditionTrack>)>;

  ~RenditionManager();
  RenditionManager(scoped_refptr<MultivariantPlaylist> playlist,
                   SelectedCB on_variant_selected,
                   IsTypeSupportedCallback is_type_supported_cb);

  void UpdatePlayerResolution(const gfx::Size& resolution);
  void UpdateNetworkSpeed(uint64_t network_bps);

  void SetAbrAlgorithmForTesting(std::unique_ptr<ABRAlgorithm> abr_algorithm);

  // Uses player state and user preferences to trigger `on_variant_selected`
  // calls with preferred playback uris.
  void Reselect(SelectedCallonce callback);

  // A nullopt means that no preference is set, and automatic selection will
  // take over.
  void SetPreferredAudioRendition(std::optional<MediaTrack::Id> track_id);
  void SetPreferredVideoRendition(std::optional<MediaTrack::Id> track_id);

  bool HasSelectableVariants() const { return !selectable_variants_.empty(); }

  std::vector<MediaTrack> GetSelectableVideoRenditions() const;
  std::vector<MediaTrack> GetSelectableAudioRenditions() const;

 private:
  const VariantStream* SelectBestVariant() const;

  // The playlist owns all VariantStream and Rendition instances, which is
  // what allows us to store raw_ptr references to those throughout the rest
  // of the member variables here.
  scoped_refptr<MultivariantPlaylist> playlist_;

  // Fired whenever a variant or rendition changes.
  SelectedCB reselect_cb_;

  std::unique_ptr<ABRAlgorithm> abr_algorithm_;

  // A sorted list of variants from {least -> most} preferrential.
  std::vector<raw_ptr<const VariantStream>> selectable_variants_;
  std::vector<MediaTrack> selectable_variant_tracks_;

  // The currently selected variant stream.
  raw_ptr<const VariantStream> active_variant_ = nullptr;

  // User selection preferences. The selection algorithm attempts to respect
  // the choice here even if underlying conditions change.
  std::optional<RenditionGroup::RenditionTrack> preferred_extra_rendition_;

  // The actively selected renditions.
  std::optional<RenditionGroup::RenditionTrack> selected_primary_;
  std::optional<RenditionGroup::RenditionTrack> selected_extra_;

  // This selection of variants are entirely audio.
  bool audio_only_ = false;

  // Playback qualities not tied to a specific variant.
  gfx::Size player_resolution_ = {limits::kMaxDimension, limits::kMaxDimension};
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_
