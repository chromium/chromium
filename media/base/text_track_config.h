// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_TEXT_TRACK_CONFIG_H_
#define MEDIA_BASE_TEXT_TRACK_CONFIG_H_

#include <string>

#include "media/base/media_export.h"

namespace media {

// Specifies the varieties of text tracks.
enum TextKind {
  kTextSubtitles,
  kTextCaptions,
  kTextDescriptions,
  kTextMetadata,
  kTextChapters,
  kTextNone
};

class MEDIA_EXPORT TextTrackConfig {
 public:
  TextTrackConfig();
  TextTrackConfig(const TextTrackConfig& other);
  TextTrackConfig& operator=(const TextTrackConfig& other);
  TextTrackConfig(TextKind kind,
                  const std::string& label,
                  const std::string& language,
                  const std::string& id);

  // Returns true if all fields in |config| match this config.
  bool Matches(const TextTrackConfig& config) const;

  TextKind kind() const { return kind_; }
  const std::string& label() const { return label_; }
  const std::string& language() const { return language_; }
  const std::string& id() const { return id_; }

  static TextKind ConvertKind(const std::string& kind);

 private:
  TextKind kind_;
  std::string label_;
  std::string language_;
  std::string id_;
};

}  // namespace media

#endif  // MEDIA_BASE_TEXT_TRACK_CONFIG_H_
