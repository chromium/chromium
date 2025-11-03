// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_RENDITION_H_
#define MEDIA_FORMATS_HLS_RENDITION_H_

#include <list>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/rendition_group.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MEDIA_EXPORT Rendition {
 public:
  struct CtorArgs;

  using Group = RenditionGroup;
  explicit Rendition(base::PassKey<Group>, CtorArgs args);
  static Rendition CreateRenditionForTesting(CtorArgs args);

  Rendition(const Rendition&) = delete;
  Rendition(Rendition&&);
  Rendition& operator=(const Rendition&) = delete;
  Rendition& operator=(Rendition&&) = delete;
  ~Rendition();

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
  explicit Rendition(CtorArgs args);

  std::optional<GURL> uri_;
  std::string name_;
  std::optional<std::string> language_;
  std::optional<std::string> associated_language_;
  std::optional<types::StableId> stable_rendition_id_;
  std::vector<std::string> characteristics_;
  std::optional<types::AudioChannels> channels_;
  bool autoselect_ = false;

  // TODO(crbug.com/314834756): Consider supporting BIT-DEPTH and SAMPLE-RATE
};

struct Rendition::CtorArgs {
  decltype(Rendition::uri_) uri;
  decltype(Rendition::name_) name;
  decltype(Rendition::language_) language;
  decltype(Rendition::associated_language_) associated_language;
  decltype(Rendition::stable_rendition_id_) stable_rendition_id;
  decltype(Rendition::channels_) channels;
  decltype(Rendition::autoselect_) autoselect;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_RENDITION_H_
