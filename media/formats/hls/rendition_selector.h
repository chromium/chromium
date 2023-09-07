// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_RENDITION_SELECTOR_H_
#define MEDIA_FORMATS_HLS_RENDITION_SELECTOR_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "media/base/media_export.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace media::hls {

class MultivariantPlaylist;
class VariantStream;
class AudioRendition;

// Class responsible for tracking playability state of all variants and
// renditions in a multivariant playlist.
class MEDIA_EXPORT RenditionSelector {
 public:
  // We want to ask if a codec string is supported, but also if it contains
  // audio, video, or both types of content, allowing us to sort our variants
  // into groups.
  enum CodecSupportType {
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
      base::RepeatingCallback<CodecSupportType(base::StringPiece,
                                               base::span<const std::string>)>;

  // Contains the set of constraints that may be used when selecting an audio
  // rendition, in the order they're used to filter available renditions.
  struct AudioPlaybackPreferences {
    // The preferred audio language, in the format given by
    // https://datatracker.ietf.org/doc/html/rfc5646. If this is
    // `absl::nullopt` or the language does not exist in any rendition, the
    // playlist's default is used.
    absl::optional<base::StringPiece> language;

    // The preferred number of audio channels. If this is `absl::nullopt` or
    // does not exist in any rendition that meets the previous preference(s),
    // its ignored.
    absl::optional<uint32_t> channel_count;
  };

  // Contains the set of constraints and values related to video playback
  // on the current system.
  struct VideoPlaybackPreferences {
    // The max bitrate supported for smooth playback.
    absl::optional<types::DecimalInteger> max_smooth_bitrate;

    // The max resolution for the player - this might change on player resize,
    // but generally there's no point in playing a 4k video on a 1080p screen.
    absl::optional<gfx::Size> video_player_resolution;
  };

  // Contains the preferred video variant, and optionally a preferred audio
  // rendition if the preferred variant has an audio group associated.
  // If there is no associated audio group with the selected rendition, then
  // the variant default will be used.
  struct PreferredVariants {
    // Use this playlist for video, and use it for audio too if the audio-only
    // variant is nullptr.
    raw_ptr<const VariantStream> selected_variant = nullptr;

    // Use this variant for audio content if it is not nullptr.
    raw_ptr<const VariantStream> audio_override_variant = nullptr;
    raw_ptr<const AudioRendition> audio_override_rendition = nullptr;
  };

  RenditionSelector(scoped_refptr<MultivariantPlaylist> playlist,
                    IsTypeSupportedCallback is_type_supported_cb);
  RenditionSelector(const RenditionSelector&);
  RenditionSelector(RenditionSelector&&);
  RenditionSelector& operator=(const RenditionSelector&);
  RenditionSelector& operator=(RenditionSelector&&);
  ~RenditionSelector();

  // Select a preferred variant based on bandwidth constraints, codec support,
  // and video resolution. Unsupported codecs will never be considered for
  // selection, while resolution and bandwidth constraints that are above
  // the optionally provided maximums might still be considered if nothing else
  // is available. If scores are provided for variants, then the highest scoring
  // variant from among the possibilities is selected.
  PreferredVariants GetPreferredVariants(
      VideoPlaybackPreferences video_preferences,
      AudioPlaybackPreferences audio_preferences) const;

 private:
  const AudioRendition* TryFindAudioOverride(
      AudioPlaybackPreferences audio_preferences,
      const VariantStream* variant) const;

  scoped_refptr<MultivariantPlaylist> playlist_;

  // All variants which we suspect have an audio component.
  std::vector<const VariantStream*> audio_variants_;

  // All variants which we suspect have a video component.
  std::vector<const VariantStream*> video_variants_;

  // The default audio language that was present in the manifest.
  // This is a singular value, relying on the fact the spec says that all groups
  // should have the same set of renditions differing only by URI and CHANNELS.
  // If this is `absl::nullopt`, there was no playable audio
  // rendition with the DEFAULT=YES and LANGUAGE attributes. If this is an empty
  // string, a default rendition exists but it had no LANGUAGE attribute.
  absl::optional<base::StringPiece> default_audio_language_;
};

}  // namespace media::hls

#endif
