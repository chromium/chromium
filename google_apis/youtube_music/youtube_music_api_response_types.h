// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_RESPONSE_TYPES_H_
#define GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_RESPONSE_TYPES_H_

#include <memory>
#include <string>
#include <vector>

#include "url/gurl.h"

namespace base {
template <class StructType>
class JSONValueConverter;
class Value;
}  // namespace base

namespace google_apis::youtube_music {

// Image object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/Image
class Image {
 public:
  Image();
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
  ~Image();

  static void RegisterJSONConverter(base::JSONValueConverter<Image>* converter);

  int width() const { return width_; }
  int height() const { return height_; }
  const GURL& url() const { return url_; }

  std::string ToString() const;

 private:
  int width_;
  int height_;
  GURL url_;
};

// Owner object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/playlists#PlaylistOwner
class Owner {
 public:
  Owner();
  Owner(const Owner&) = delete;
  Owner& operator=(const Owner&) = delete;
  ~Owner();

  static void RegisterJSONConverter(base::JSONValueConverter<Owner>* converter);

  const std::string& title() const { return title_; }

  std::string ToString() const;

 private:
  std::string title_;
};

// Playlist object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/playlists#Playlist
class Playlist {
 public:
  Playlist();
  Playlist(const Playlist&) = delete;
  Playlist& operator=(const Playlist&) = delete;
  ~Playlist();

  static void RegisterJSONConverter(
      base::JSONValueConverter<Playlist>* converter);

  static std::unique_ptr<Playlist> CreateFrom(const base::Value& value);

  const std::string& name() const { return name_; }
  const std::string& title() const { return title_; }
  const std::string& description() const { return description_; }
  int item_count() const { return item_count_; }
  const std::vector<std::unique_ptr<Image>>& images() const { return images_; }
  std::vector<std::unique_ptr<Image>>* mutable_images() { return &images_; }
  const Owner& owner() const { return owner_; }

  std::string ToString() const;

 private:
  std::string name_;
  std::string title_;
  std::string description_;
  int item_count_;
  std::vector<std::unique_ptr<Image>> images_;
  Owner owner_;
};

// Music recommendation object from the API response. For object details, check
// below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load#musicrecommendation
class MusicRecommendation {
 public:
  MusicRecommendation();
  MusicRecommendation(const MusicRecommendation&) = delete;
  MusicRecommendation& operator=(const MusicRecommendation&) = delete;
  ~MusicRecommendation();

  static void RegisterJSONConverter(
      base::JSONValueConverter<MusicRecommendation>* converter);

  const Playlist& playlist() const { return playlist_; }

  std::string ToString() const;

 private:
  Playlist playlist_;
};

// Music section object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load
class MusicSection {
 public:
  MusicSection();
  MusicSection(const MusicSection&) = delete;
  MusicSection& operator=(const MusicSection&) = delete;
  ~MusicSection();

  static void RegisterJSONConverter(
      base::JSONValueConverter<MusicSection>* converter);

  static std::unique_ptr<MusicSection> CreateFrom(const base::Value& value);

  const std::string& name() const { return name_; }
  const std::string& title() const { return title_; }
  const std::vector<std::unique_ptr<MusicRecommendation>>&
  music_recommendations() const {
    return music_recommendations_;
  }
  std::vector<std::unique_ptr<MusicRecommendation>>*
  mutable_music_recommendations() {
    return &music_recommendations_;
  }

  std::string ToString() const;

 private:
  std::string name_;
  std::string title_;
  std::vector<std::unique_ptr<MusicRecommendation>> music_recommendations_;
};

// Top level music recommendations object from the API response. For object
// details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load
class TopLevelMusicRecommendation {
 public:
  TopLevelMusicRecommendation();
  TopLevelMusicRecommendation(const TopLevelMusicRecommendation&) = delete;
  TopLevelMusicRecommendation& operator=(const TopLevelMusicRecommendation&) =
      delete;
  ~TopLevelMusicRecommendation();

  static void RegisterJSONConverter(
      base::JSONValueConverter<TopLevelMusicRecommendation>* converter);

  static std::unique_ptr<TopLevelMusicRecommendation> CreateFrom(
      const base::Value& value);

  const MusicSection& music_section() const { return music_section_; }

  std::string ToString() const;

 private:
  MusicSection music_section_;
};

// Top level music section and music recommendations object from the API
// response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load
class TopLevelMusicRecommendations {
 public:
  TopLevelMusicRecommendations();
  TopLevelMusicRecommendations(const TopLevelMusicRecommendations&) = delete;
  TopLevelMusicRecommendations& operator=(const TopLevelMusicRecommendations&) =
      delete;
  ~TopLevelMusicRecommendations();

  static void RegisterJSONConverter(
      base::JSONValueConverter<TopLevelMusicRecommendations>* converter);

  static std::unique_ptr<TopLevelMusicRecommendations> CreateFrom(
      const base::Value& value);

  const std::vector<std::unique_ptr<TopLevelMusicRecommendation>>&
  top_level_music_recommendations() const {
    return top_level_music_recommendations_;
  }
  std::vector<std::unique_ptr<TopLevelMusicRecommendation>>*
  mutable_top_level_music_recommendations() {
    return &top_level_music_recommendations_;
  }

  std::string ToString() const;

 private:
  std::vector<std::unique_ptr<TopLevelMusicRecommendation>>
      top_level_music_recommendations_;
};

// Artist reference object from the API response. For object details, check
// below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/ArtistReference
class ArtistReference {
 public:
  ArtistReference();
  ArtistReference(const ArtistReference&) = delete;
  ArtistReference& operator=(const ArtistReference&) = delete;
  ~ArtistReference();

  static void RegisterJSONConverter(
      base::JSONValueConverter<ArtistReference>* converter);

  static std::unique_ptr<ArtistReference> CreateFrom(const base::Value& value);

  const std::string& artist() const { return artist_; }
  const std::string& title() const { return title_; }

  std::string ToString() const;

 private:
  std::string artist_;
  std::string title_;
};

// Track object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/tracks#Track
class Track {
 public:
  Track();
  Track(const Track&) = delete;
  Track& operator=(const Track&) = delete;
  ~Track();

  static void RegisterJSONConverter(base::JSONValueConverter<Track>* converter);

  static std::unique_ptr<Track> CreateFrom(const base::Value& value);

  const std::string& name() const { return name_; }
  const std::string& title() const { return title_; }
  const std::string& duration() const { return duration_; }
  const std::string& explicit_type() const { return explicit_type_; }
  const std::vector<std::unique_ptr<Image>>& images() const { return images_; }
  std::vector<std::unique_ptr<Image>>* mutable_images() { return &images_; }
  const std::vector<std::unique_ptr<ArtistReference>>& artist_references()
      const {
    return artist_references_;
  }
  std::vector<std::unique_ptr<ArtistReference>>* mutable_artist_references() {
    return &artist_references_;
  }

  std::string ToString() const;

 private:
  std::string name_;
  std::string title_;
  std::string duration_;
  std::string explicit_type_;
  std::vector<std::unique_ptr<Image>> images_;
  std::vector<std::unique_ptr<ArtistReference>> artist_references_;
};

// Queue item object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#QueueItem
class QueueItem {
 public:
  QueueItem();
  QueueItem(const QueueItem&) = delete;
  QueueItem& operator=(const QueueItem&) = delete;
  ~QueueItem();

  static void RegisterJSONConverter(
      base::JSONValueConverter<QueueItem>* converter);

  static std::unique_ptr<QueueItem> CreateFrom(const base::Value& value);

  const Track& track() const { return track_; }

  std::string ToString() const;

 private:
  Track track_;
};

// Stream object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#Stream
class Stream {
 public:
  Stream();
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  ~Stream();

  static void RegisterJSONConverter(
      base::JSONValueConverter<Stream>* converter);

  static std::unique_ptr<Stream> CreateFrom(const base::Value& value);

  const std::string& playback_reporting_token() const {
    return playback_reporting_token_;
  }
  const GURL& url() const { return url_; }

  std::string ToString() const;

 private:
  GURL url_;
  std::string playback_reporting_token_;
};

// Playback manifest object from the API response. For object details, check
// below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#PlaybackManifest
class PlaybackManifest {
 public:
  PlaybackManifest();
  PlaybackManifest(const PlaybackManifest&) = delete;
  PlaybackManifest& operator=(const PlaybackManifest&) = delete;
  ~PlaybackManifest();

  static void RegisterJSONConverter(
      base::JSONValueConverter<PlaybackManifest>* converter);

  static std::unique_ptr<PlaybackManifest> CreateFrom(const base::Value& value);

  const std::vector<std::unique_ptr<Stream>>& streams() const {
    return streams_;
  }
  std::vector<std::unique_ptr<Stream>>* mutable_streams() { return &streams_; }

  std::string ToString() const;

 private:
  std::vector<std::unique_ptr<Stream>> streams_;
};

// Playback context object from the API response. For object details, check
// below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#PlaybackContext
class PlaybackContext {
 public:
  PlaybackContext();
  PlaybackContext(const PlaybackContext&) = delete;
  PlaybackContext& operator=(const PlaybackContext&) = delete;
  ~PlaybackContext();

  static void RegisterJSONConverter(
      base::JSONValueConverter<PlaybackContext>* converter);

  static std::unique_ptr<PlaybackContext> CreateFrom(const base::Value& value);

  const QueueItem& queue_item() const { return queue_item_; }
  const PlaybackManifest& playback_manifest() const {
    return playback_manifest_;
  }

  std::string ToString() const;

 private:
  QueueItem queue_item_;
  PlaybackManifest playback_manifest_;
};

// Queue object from the API response. For object details, check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues#Queue
class Queue {
 public:
  Queue();
  Queue(const Queue&) = delete;
  Queue& operator=(const Queue&) = delete;
  ~Queue();

  static void RegisterJSONConverter(base::JSONValueConverter<Queue>* converter);

  static std::unique_ptr<Queue> CreateFrom(const base::Value& value);

  const std::string name() const { return name_; }
  int size() const { return size_; }
  const PlaybackContext& playback_context() const { return playback_context_; }

  std::string ToString() const;

 private:
  std::string name_;
  int size_;
  PlaybackContext playback_context_;
};

// Playback queue container object from the API response. For object details,
// check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues/next
class QueueContainer {
 public:
  QueueContainer();
  QueueContainer(const QueueContainer&) = delete;
  QueueContainer& operator=(const QueueContainer&) = delete;
  ~QueueContainer();

  static void RegisterJSONConverter(
      base::JSONValueConverter<QueueContainer>* converter);

  static std::unique_ptr<QueueContainer> CreateFrom(const base::Value& value);

  const Queue& queue() const { return queue_; }

  std::string ToString() const;

 private:
  Queue queue_;
};

// Report playback result object from the API response. For object details,
// check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/reports/playback#response-body
class ReportPlaybackResult {
 public:
  ReportPlaybackResult();
  ReportPlaybackResult(const ReportPlaybackResult&) = delete;
  ReportPlaybackResult& operator=(const ReportPlaybackResult&) = delete;
  ~ReportPlaybackResult();

  static void RegisterJSONConverter(
      base::JSONValueConverter<ReportPlaybackResult>* converter);

  static std::unique_ptr<ReportPlaybackResult> CreateFrom(
      const base::Value& value);

  const std::string& playback_reporting_token() const {
    return playback_reporting_token_;
  }

  std::string ToString() const;

 private:
  std::string playback_reporting_token_;
};

}  // namespace google_apis::youtube_music

#endif  // GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_RESPONSE_TYPES_H_
