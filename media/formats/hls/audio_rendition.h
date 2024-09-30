// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_AUDIO_RENDITION_H_
#define MEDIA_FORMATS_HLS_AUDIO_RENDITION_H_

#include <list>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace media::hls {

class AudioRenditionGroup;
class MultivariantPlaylist;

class MEDIA_EXPORT AudioRendition {
 public:
  struct CtorArgs;
  explicit AudioRendition(base::PassKey<AudioRenditionGroup>, CtorArgs args);
  AudioRendition(const AudioRendition&) = delete;
  AudioRendition(AudioRendition&&);
  AudioRendition& operator=(const AudioRendition&) = delete;
  AudioRendition& operator=(AudioRendition&&) = delete;
  ~AudioRendition();

  // Returns the URI for the media playlist of this rendition.
  const std::optional<GURL>& GetUri() const { return uri_; }

  // Returns the name of this rendition, which must be unique within the group
  // containing this rendition.
  const std::string& GetName() const { return name_; }

  // Returns the language of this rendition.
  const std::optional<std::string>& GetLanguage() const { return language_; }

  // Returns an associated language of this rendition.
  const std::optional<std::string>& GetAssociatedLanguage() const {
    return associated_language_;
  }

  // Returns a stable identifier for the URI of this rendition.
  const std::optional<types::StableId>& GetStableRenditionId() const {
    return stable_rendition_id_;
  }

  // Returns the list of media characteristic tags associated with this
  // rendition.
  const std::vector<std::string>& GetCharacteristics() const {
    return characteristics_;
  }

  // Returns channel information for this rendition.
  const std::optional<types::AudioChannels>& GetChannels() const {
    return channels_;
  }

  // Returns whether this rendition may be considered in the absence of a
  // user preference indicating otherwise.
  bool MayAutoSelect() const { return autoselect_; }

 private:
  std::optional<GURL> uri_;
  std::string name_;
  std::optional<std::string> language_;
  std::optional<std::string> associated_language_;
  std::optional<types::StableId> stable_rendition_id_;
  std::vector<std::string> characteristics_;
  std::optional<types::AudioChannels> channels_;
  bool autoselect_ = false;
};

class MEDIA_EXPORT AudioRenditionGroup
    : public base::RefCounted<AudioRenditionGroup> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  explicit AudioRenditionGroup(base::PassKey<MultivariantPlaylist>,
                               std::string id);
  AudioRenditionGroup(const AudioRenditionGroup&) = delete;
  AudioRenditionGroup(AudioRenditionGroup&&) = delete;
  AudioRenditionGroup& operator=(const AudioRenditionGroup&) = delete;
  AudioRenditionGroup& operator=(AudioRenditionGroup&&) = delete;

  // Adds a rendition specified by the given `XMediaTag` to this group. The
  // caller is responsible for ensuring that the rendition passed in is
  // individually valid, has `type == MediaType::kAudio`, and belongs to this
  // group. If the rendition is invalid in the context of the group, an error
  // will be returned.
  ParseStatus::Or<absl::monostate> AddRendition(
      base::PassKey<MultivariantPlaylist>,
      XMediaTag tag,
      const GURL& playlist_uri);

  // Returns the id of this audio rendition group.
  const std::string& GetId() const { return id_; }

  // Returns the set of renditions that belong to this group, in the order they
  // appeared in the manifest.
  const std::list<AudioRendition>& GetRenditions() const { return renditions_; }

  // Looks up the renditions within this group identified by the given name.
  // If no such renditions exists, returns `nullptr`.
  const AudioRendition* GetRendition(std::string_view name) const;

  // Returns the rendition which was specified with the DEFAULT=YES attribute.
  // If no such rendition was in this group, returns `nullptr`;
  const AudioRendition* GetDefaultRendition() const {
    return default_rendition_;
  }

 private:
  friend base::RefCounted<AudioRenditionGroup>;
  ~AudioRenditionGroup();

  std::string id_;

  // Set of renditions within this group, in the order they appeared in the
  // manifest. Using a `std::list` as opposed to a `std::vector` to ensure
  // pointer stability.
  std::list<AudioRendition> renditions_;

  // Set of renditions within this group, keyed by their NAME attribute.
  base::flat_map<std::string, raw_ptr<const AudioRendition, CtnExperimental>>
      renditions_map_;

  // Default rendition, `nullptr` if none.
  raw_ptr<const AudioRendition> default_rendition_ = nullptr;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_AUDIO_RENDITION_H_
