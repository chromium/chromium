// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_H_
#define MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_H_

#include <string_view>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/playlist.h"
#include "media/formats/hls/tag_recorder.h"
#include "media/formats/hls/types.h"
#include "media/formats/hls/variable_dictionary.h"
#include "url/gurl.h"

namespace media::hls {

class VariantStream;

class MEDIA_EXPORT MultivariantPlaylist final : public Playlist {
 public:
  MultivariantPlaylist(base::PassKey<MultivariantPlaylist>,
                       GURL uri,
                       types::DecimalInteger version,
                       bool independent_segments,
                       std::vector<VariantStream> variants,
                       VariableDictionary variable_dictionary);
  MultivariantPlaylist(const MultivariantPlaylist&) = delete;
  MultivariantPlaylist(MultivariantPlaylist&&) = delete;
  MultivariantPlaylist& operator=(const MultivariantPlaylist&) = delete;
  MultivariantPlaylist& operator=(MultivariantPlaylist&&) = delete;

  // Returns all variants described by this playlist.
  const std::vector<VariantStream>& GetVariants() const { return variants_; }

  // Returns the dictionary of variables defined by this playlist.
  const VariableDictionary& GetVariableDictionary() const {
    return variable_dictionary_;
  }

  // `Playlist` implementation
  Kind GetKind() const override;

  // Attempts to parse the multivariant playlist represented by `source`. `uri`
  // must be a valid, non-empty GURL referring to the URI of this playlist.
  // `version` is the HLS version expected to be given by an `EXT-X-VERSION` tag
  // in this playlist (or `Playlist::kDefaultVersion` if none), which may be
  // determined via `Playlist::IdentifyPlaylist`. If the playlist source is
  // invalid, returns an error.
  static ParseStatus::Or<scoped_refptr<MultivariantPlaylist>> Parse(
      std::string_view source,
      GURL uri,
      types::DecimalInteger version,
      TagRecorder* tag_recorder = nullptr);

 private:
  ~MultivariantPlaylist() override;

  std::vector<VariantStream> variants_;
  VariableDictionary variable_dictionary_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_MULTIVARIANT_PLAYLIST_H_
