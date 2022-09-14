// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/text_track_config.h"

namespace media {

TextTrackConfig::TextTrackConfig()
    : kind_(kTextNone) {
}

TextTrackConfig::TextTrackConfig(TextKind kind,
                                 const std::string& label,
                                 const std::string& language,
                                 const std::string& id)
    : kind_(kind),
      label_(label),
      language_(language),
      id_(id) {
}

TextTrackConfig::TextTrackConfig(const TextTrackConfig&) = default;

TextTrackConfig& TextTrackConfig::operator=(const TextTrackConfig&) = default;

bool TextTrackConfig::Matches(const TextTrackConfig& config) const {
  return config.kind() == kind_ &&
         config.label() == label_ &&
         config.language() == language_ &&
         config.id() == id_;
}

// static
TextKind TextTrackConfig::ConvertKind(const std::string& str) {
  if (str == "subtitles")
    return kTextSubtitles;
  if (str == "captions")
    return kTextCaptions;
  if (str == "descriptions")
    return kTextDescriptions;
  if (str == "chapters")
    return kTextChapters;
  if (str == "metadata")
    return kTextMetadata;
  return kTextNone;
}

}  // namespace media
