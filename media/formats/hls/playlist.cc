// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/hls/playlist.h"

#include "media/formats/hls/items.h"
#include "media/formats/hls/playlist_common.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

Playlist::Playlist(GURL uri,
                   types::DecimalInteger version,
                   bool independent_segments)
    : uri_(std::move(uri)),
      version_(version),
      independent_segments_(independent_segments) {}

Playlist::~Playlist() = default;

// static
ParseStatus::Or<Playlist::Identification> Playlist::IdentifyPlaylist(
    std::string_view source) {
  std::optional<Kind> playlist_kind;
  std::optional<XVersionTag> version_tag;

  // Iterate through playlist lines until we can identify the version and the
  // playlist kind.
  SourceLineIterator iter(source);
  while (!playlist_kind.has_value() || !version_tag.has_value()) {
    auto item_result = GetNextLineItem(&iter);
    if (!item_result.has_value()) {
      auto error = std::move(item_result).error();

      // Only tolerated error is EOF
      if (error.code() == ParseStatusCode::kReachedEOF) {
        break;
      }

      return error;
    }

    auto item = std::move(item_result).value();
    if (auto* tag = absl::get_if<TagItem>(&item)) {
      // We can't make any assumptions on unknown tags
      if (!tag->GetName().has_value()) {
        continue;
      }
      const auto tag_name = tag->GetName().value();

      // If this is a version tag, try parsing it to determine the version
      if (tag_name == ToTagName(CommonTagName::kXVersion)) {
        auto error = ParseUniqueTag(*tag, version_tag);
        if (error.has_value()) {
          return std::move(error).value();
        }

        // Ensure that the version is supported by this implementation
        if (version_tag->version < Playlist::kMinSupportedVersion ||
            version_tag->version > Playlist::kMaxSupportedVersion) {
          return ParseStatusCode::kPlaylistHasUnsupportedVersion;
        }

        continue;
      }

      // Otherwise use the tag's kind to identify playlist kind without
      // attempting to parse it.
      switch (GetTagKind(tag_name)) {
        case TagKind::kCommonTag:
          break;
        case TagKind::kMultivariantPlaylistTag:
          if (playlist_kind == Kind::kMediaPlaylist) {
            return ParseStatusCode::kMediaPlaylistHasMultivariantPlaylistTag;
          }
          playlist_kind = Kind::kMultivariantPlaylist;
          break;
        case TagKind::kMediaPlaylistTag:
          if (playlist_kind == Kind::kMultivariantPlaylist) {
            return ParseStatusCode::kMultivariantPlaylistHasMediaPlaylistTag;
          }
          playlist_kind = Kind::kMediaPlaylist;
          break;
      }
    }
  }

  return Identification{
      // If the playlist did not contain a version tag, version is implicitly
      // `kDefaultVersion`.
      .version = version_tag ? version_tag->version : kDefaultVersion,

      // Media playlists must contain the EXT-X-TARGETDURATION tag, so if we
      // never encountered a media playlist tag it must be an (empty)
      // multivariant playlist.
      .kind =
          playlist_kind ? playlist_kind.value() : Kind::kMultivariantPlaylist,
  };
}

}  // namespace media::hls
