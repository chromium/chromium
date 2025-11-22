// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_HLS_RENDITION_GROUP_H_
#define MEDIA_FORMATS_HLS_RENDITION_GROUP_H_

#include <list>
#include <optional>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "media/base/media_export.h"
#include "media/base/media_track.h"
#include "media/formats/hls/parse_status.h"
#include "media/formats/hls/tags.h"
#include "media/formats/hls/types.h"
#include "url/gurl.h"

namespace media::hls {

class MultivariantPlaylist;
class Rendition;

class MEDIA_EXPORT RenditionGroup : public base::RefCounted<RenditionGroup> {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  RenditionGroup(base::PassKey<MultivariantPlaylist>,
                 std::optional<std::string> id);

  RenditionGroup(const RenditionGroup&) = delete;
  RenditionGroup(RenditionGroup&&) = delete;
  RenditionGroup& operator=(const RenditionGroup&) = delete;
  RenditionGroup& operator=(RenditionGroup&&) = delete;

  using RenditionTrack = std::tuple<MediaTrack, raw_ptr<const Rendition>>;
  using RenditionTrackId = base::IdType<class RenditionTrackIdTag, uint64_t, 0>;

  // Adds a rendition specified by the given `XMediaTag` to this group. The
  // caller is responsible for ensuring that the rendition passed in is
  // individually valid, has a type matching Rendition::Type, and belongs to
  // this group. If the rendition is invalid in the context of the group, an
  // error will be returned.
  ParseStatus::Or<std::monostate> AddRendition(
      base::PassKey<MultivariantPlaylist>,
      XMediaTag tag,
      const GURL& playlist_uri,
      RenditionTrackId unique_id);

  // Adds the "virtual" rendition created from the required default URL in a
  // VariantStream. The label, ID, and name are all "default".
  RenditionTrack MakeImplicitRendition(base::PassKey<MultivariantPlaylist>,
                                       MediaType type,
                                       const GURL& default_rendition_uri,
                                       RenditionTrackId unique_id);

  // Given a rendition track, try to find the track in this group which best
  // matches it's characteristics. If the provided rendition is a member of
  // this group, it will be returned. If nullopt is provided, then return any
  // "most preferential" rendition.
  const std::optional<RenditionTrack> MostSimilar(
      const std::optional<RenditionTrack>& to) const;

  // Look up a rendition with a matching track id.
  const std::optional<RenditionTrack> GetRenditionById(
      const MediaTrack::Id& id) const;

  // Returns the id of this rendition group.
  const std::optional<std::string>& GetIdForTesting() const { return id_; }

  // Returns the set of renditions that belong to this group, in the order they
  // appeared in the manifest.
  const std::list<Rendition>& GetRenditionsForTesting() const {
    return renditions_;
  }

  const std::vector<MediaTrack>& GetTracks() const { return tracks_; }

  bool HasTracks() const { return !tracks_.empty(); }

  // Returns the rendition which was specified with the DEFAULT=YES attribute.
  const std::optional<RenditionTrack> GetDefaultRendition() const {
    return default_rendition_;
  }

 private:
  friend base::RefCounted<RenditionGroup>;
  ~RenditionGroup();

  std::optional<std::string> id_;

  // Set of renditions within this group, in the order they appeared in the
  // manifest. Using a `std::list` as opposed to a `std::vector` to ensure
  // pointer stability.
  std::list<Rendition> renditions_;

  // The list of media tracks associated with our renditions.
  std::vector<MediaTrack> tracks_;

  base::flat_map<MediaTrack::Id, RenditionTrack> renditions_map_;

  // If one of the renditions in this group has the tag DEFAULT=YES, it is set
  // here, and should be chosen by `MostSimilar` when a nullopt parameter is
  // provided.
  std::optional<RenditionTrack> default_rendition_;
};

}  // namespace media::hls

#endif  // MEDIA_FORMATS_HLS_RENDITION_GROUP_H_
