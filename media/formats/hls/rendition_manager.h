// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_
#define MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_

#include <deque>
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
#include "media/formats/hls/types.h"
#include "media/formats/hls/variant_stream.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace media::hls {

class MultivariantPlaylist;
class AudioRendition;
class AudioRenditionGroup;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class AdaptationReason {
  kUserSelection = 0,
  kResolutionChange = 1,
  kNetworkUpgrade = 2,
  kNetworkDowngrade = 3,

  kMaxValue = kNetworkDowngrade,
};

// Class responsible for tracking playability state of all variants and
// renditions in a multivariant playlist. It will always select a preferred
// variant, and then from within that variant, select an optional audio-override
// rendition. The selection depends on user preference, network speed, frame
// drops, underflow events, and player resolution.
class MEDIA_EXPORT RenditionManager {
 public:
  using VariantID = base::IdType32<VariantStream>;
  using RenditionID = base::IdType32<AudioRendition>;

  // We want to ask if a codec string is supported, but also if it contains
  // audio, video, or both types of content, allowing us to sort our variants
  // into groups.
  enum class CodecSupportType {
    kUnsupported,
    kSupportedAudioVideo,
    kSupportedAudioOnly,
    kSupportedVideoOnly,
  };

  // A SelectableOption consists of an ID representing either a variant or a
  // rendition, as well as a string that should be displayed to a user in a menu
  // for selecting a preferred rendition or variant.
  template <typename T>
  using SelectableOption = std::tuple<T, std::string>;

  // Callback used to query whether the given MIME+codec string is supported,
  // and what types of content we can expect.
  // The first argument is the name of the container, ie "video/mp4" or
  // "video/mp2t", and the second argument is a list of codecs to check support
  // for, such as ["mp4a.40.2", "avc1.4d4015"]
  using IsTypeSupportedCallback =
      base::RepeatingCallback<CodecSupportType(std::string_view,
                                               base::span<const std::string>)>;

  // The VariantStream ptr can be null if there is not supposed to be a change
  // in the URI of the primary variant when this callback is run.
  // The AudioRendition ptr can be null if there is no audio override rendition
  // selected.
  using SelectedCB = base::RepeatingCallback<
      void(AdaptationReason, const VariantStream*, const AudioRendition*)>;
  using SelectedCallonce =
      base::OnceCallback<void(const VariantStream*, const AudioRendition*)>;

  ~RenditionManager();
  RenditionManager(scoped_refptr<MultivariantPlaylist> playlist,
                   SelectedCB on_variant_selected,
                   IsTypeSupportedCallback is_type_supported_cb);

  void UpdatePlayerResolution(const gfx::Size& resolution);
  void UpdateNetworkSpeed(uint64_t network_bps);

  // After initialization, is there anything to select?
  bool HasAnyVariants() const;

  std::vector<MediaTrack> GetSelectableVariants() const;
  std::vector<MediaTrack> GetSelectableRenditions() const;

  // Uses player state and user preferences to trigger `on_variant_selected`
  // calls with preferred playback uris.
  void Reselect(SelectedCallonce callback);

  // Set preferred variants. A nullopt means that no preference is set, and
  // automatic selection can take place.
  void SetPreferredAudioRendition(std::optional<MediaTrack::Id> rendition_id);
  void SetPreferredVariant(std::optional<MediaTrack::Id> variant_id);

 private:
  struct UpdatedSelections {
    ~UpdatedSelections();
    UpdatedSelections();
    UpdatedSelections(const UpdatedSelections&);
    std::optional<VariantID> variant;
    std::optional<RenditionID> audio_rendition;
  };

  struct VariantMetadata {
    ~VariantMetadata();
    VariantMetadata(const VariantMetadata&);
    VariantMetadata(const VariantStream*, const AudioRenditionGroup*);

    MediaTrack::Id track_id;
    raw_ptr<const VariantStream> stream;
    raw_ptr<const AudioRenditionGroup> audio_rendition_group;
    base::flat_set<RenditionID> audio_renditions;
  };

  struct RenditionMetadata {
    ~RenditionMetadata();
    RenditionMetadata(const RenditionMetadata&);
    explicit RenditionMetadata(const AudioRendition*);

    MediaTrack::Id track_id;
    raw_ptr<const AudioRendition> rendition;
  };

  // Called during construction. This creates the per-variant statistics
  // trackers and all the maps where variants can reference IDs.
  void InitializeVariantMaps(IsTypeSupportedCallback callback);

  // Helper for `Reselect()` which computes what the best rendition and variant
  // are, so that they can be compared to what is currently selected and a diff
  // can be returned.
  UpdatedSelections GetUpdatedSelectionIds();

  // Runs the variant selection algorithm, which tries to select the highest
  // bitrate variant which isn't precluded by some combination of player
  // resolution and network speed.
  std::optional<VariantID> SelectBestVariant();

  // Given the selected variant, select the best audio rendition that could be
  // used as an audio-override rendition. It first tries to use any
  // user-selected rendition, but if it's not available, tries to find the most
  // similar rendition to the user's selection. If there is no user selection,
  // it uses defaults and autoselect tags to determine which rendition to pick.
  std::optional<RenditionID> SelectBestRendition(VariantID,
                                                 std::optional<RenditionID>);

  // Backwards lookup of RenditionID from `selectable_renditions_`. This map is
  // usually so small that an iteration is not significantly slow.
  std::optional<RenditionID> LookupRendition(const AudioRendition* rendition);

  // Determines the set and order of format components used to generate a human
  // readable (and differentiable!) name for a variant stream.
  std::vector<VariantStream::FormatComponent> DetermineVariantStreamFormatting()
      const;

  // Selects the best rendition based with an optionally given language, and a
  // flag stating whether only renditions tagged with "AUTOSELECT=TRUE" may be
  // selected. The premise here is to prefer renditions in order of:
  //  - variant->Default (guaranteed autoselect always) if language is null
  //  - variant->Default if language matches
  //  - any rendition with matching language and matching selectability
  //  - the default rendition if no renditions languages match
  //  - any rendition with matching selectability
  //  - nothing
  std::optional<RenditionManager::RenditionID> SelectRenditionBasedOnLanguage(
      const VariantMetadata& variant,
      std::optional<std::string> maybe_language,
      bool only_autoselect);

  // The playlist owns all VariantStream and VideoRendition instances, which is
  // what allows us to store raw_ptr references to those throughout the rest
  // of the member variables here.
  scoped_refptr<MultivariantPlaylist> playlist_;

  // Fired whenever a variant or rendition changes.
  SelectedCB on_variant_selected_;

  // Internally, we use VariantID and RenditionID for key based lookups because
  // they are integer types and cheap to compare. Blink uses strings for stream
  // ID's however, so those get passed down to us in the form of MediaTrack::Id
  // instances. When we provide MediaTrack objects, we also need to be able to
  // look them up by VariantID or RenditionID.
  // The metadata maps are responsible for both the InternalID => parser struct
  // lookups, as well as the InternalID => MediaTrack::Id lookups.
  base::flat_map<VariantID, VariantMetadata> selectable_variants_;
  base::flat_map<RenditionID, RenditionMetadata> selectable_renditions_;
  base::flat_map<MediaTrack::Id, MediaTrack> track_map_;

  std::optional<VariantID> selected_variant_;
  std::optional<VariantID> preferred_variant_;

  std::optional<RenditionID> selected_audio_rendition_;
  std::optional<RenditionID> preferred_audio_rendition_;

  VariantID::Generator variant_id_gen_;
  RenditionID::Generator rendition_id_gen_;

  // Playback qualities not tied to a specific variant.
  gfx::Size player_resolution_ = {limits::kMaxDimension, limits::kMaxDimension};
  uint64_t network_bps_ = 0xFFFFFFFFFF;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_RENDITION_MANAGER_H_
