// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_response_types.h"

#include <string>

#include "base/json/json_value_converter.h"
#include "base/strings/stringprintf.h"

namespace google_apis::youtube_music {
namespace {

using ::base::JSONValueConverter;

constexpr char kApiResponseNameKey[] = "name";
constexpr char kApiResponseTitleKey[] = "title";
constexpr char kApiResponseMusicRecommendationsKey[] = "musicRecommendations";
constexpr char kApiResponseMusicSectionKey[] = "musicSection";
constexpr char kApiResponseWidthKey[] = "widthPixels";
constexpr char kApiResponseHeightKey[] = "heightPixels";
constexpr char kApiResponseUriKey[] = "uri";
constexpr char kApiResponsePlaybackReportingTokenKey[] =
    "playbackReportingToken";
constexpr char kApiResponseDescriptionKey[] = "description";
constexpr char kApiResponseItemCountKey[] = "itemCount";
constexpr char kApiResponseImagesKey[] = "images";
constexpr char kApiResponseArtistKey[] = "artist";
constexpr char kApiResponseArtistReferencesKey[] = "artistReferences";
constexpr char kApiResponseOwnerKey[] = "owner";
constexpr char kApiResponsePlaylistKey[] = "playlist";
constexpr char kApiResponseContextKey[] = "context";
constexpr char kApiResponseSizeKey[] = "size";
constexpr char kApiResponseQueueKey[] = "queue";
constexpr char kApiResponseDurationKey[] = "duration";
constexpr char kApiResponseExplicitTypeKey[] = "explicitType";
constexpr char kApiResponseTrackKey[] = "track";
constexpr char kApiResponseStreamsKey[] = "streams";
constexpr char kApiResponseItemKey[] = "item";
constexpr char kApiResponseManifestKey[] = "manifest";

bool ConvertToGurl(std::string_view input, GURL* output) {
  *output = GURL(input);
  return true;
}

}  // namespace

Image::Image() = default;
Image::~Image() = default;

// static
void Image::RegisterJSONConverter(JSONValueConverter<Image>* converter) {
  converter->RegisterIntField(kApiResponseWidthKey, &Image::width_);
  converter->RegisterIntField(kApiResponseHeightKey, &Image::height_);
  converter->RegisterCustomField<GURL>(kApiResponseUriKey, &Image::url_,
                                       &ConvertToGurl);
}

std::string Image::ToString() const {
  return base::StringPrintf("Image(width=%d, height=%d, url=\"%s\")", width_,
                            height_, url_.spec().c_str());
}

Owner::Owner() = default;
Owner::~Owner() = default;

// static
void Owner::RegisterJSONConverter(JSONValueConverter<Owner>* converter) {
  converter->RegisterStringField(kApiResponseTitleKey, &Owner::title_);
}

std::string Owner::ToString() const {
  return base::StringPrintf("Owner(title=\"%s\"", title_.c_str());
}

Playlist::Playlist() = default;
Playlist::~Playlist() = default;

// static
void Playlist::RegisterJSONConverter(JSONValueConverter<Playlist>* converter) {
  converter->RegisterStringField(kApiResponseNameKey, &Playlist::name_);
  converter->RegisterStringField(kApiResponseTitleKey, &Playlist::title_);
  converter->RegisterStringField(kApiResponseDescriptionKey,
                                 &Playlist::description_);
  converter->RegisterIntField(kApiResponseItemCountKey, &Playlist::item_count_);
  converter->RegisterRepeatedMessage<Image>(kApiResponseImagesKey,
                                            &Playlist::images_);
  converter->RegisterNestedField(kApiResponseOwnerKey, &Playlist::owner_);
}

// static
std::unique_ptr<Playlist> Playlist::CreateFrom(const base::Value& value) {
  auto playlist = std::make_unique<Playlist>();
  JSONValueConverter<Playlist> converter;
  if (!converter.Convert(value, playlist.get())) {
    DVLOG(1) << "Unable to construct `Playlist` from parsed json.";
    return nullptr;
  }
  return playlist;
}

std::string Playlist::ToString() const {
  std::string s;
  for (size_t i = 0; i < images_.size(); i++) {
    s += (i ? ", " : "") + images_[i]->ToString();
  }
  return base::StringPrintf(
      "Playlist(name=\"%s\", title=\"%s\", description=\"%s\", item_count=%d, "
      "images=[%s], owner=%s)",
      name_.c_str(), title_.c_str(), description_.c_str(), item_count_,
      s.c_str(), owner_.ToString().c_str());
}

MusicRecommendation::MusicRecommendation() = default;
MusicRecommendation::~MusicRecommendation() = default;

// static
void MusicRecommendation::RegisterJSONConverter(
    JSONValueConverter<MusicRecommendation>* converter) {
  converter->RegisterNestedField(kApiResponsePlaylistKey,
                                 &MusicRecommendation::playlist_);
}

std::string MusicRecommendation::ToString() const {
  return base::StringPrintf("MusicRecommendation(playlist=%s)",
                            playlist_.ToString().c_str());
}

MusicSection::MusicSection() = default;
MusicSection::~MusicSection() = default;

// static
void MusicSection::RegisterJSONConverter(
    JSONValueConverter<MusicSection>* converter) {
  converter->RegisterStringField(kApiResponseNameKey, &MusicSection::name_);
  converter->RegisterStringField(kApiResponseTitleKey, &MusicSection::title_);
  converter->RegisterRepeatedMessage<MusicRecommendation>(
      kApiResponseMusicRecommendationsKey,
      &MusicSection::music_recommendations_);
}

// static
std::unique_ptr<MusicSection> MusicSection::CreateFrom(
    const base::Value& value) {
  auto music_section = std::make_unique<MusicSection>();
  JSONValueConverter<MusicSection> converter;
  if (!converter.Convert(value, music_section.get())) {
    DVLOG(1) << "Unable to construct `MusicSection` from parsed json.";
    return nullptr;
  }
  return music_section;
}

std::string MusicSection::ToString() const {
  std::string s;
  for (size_t i = 0; i < music_recommendations_.size(); i++) {
    s += (i ? ", " : "") + music_recommendations_[i]->ToString();
  }
  return base::StringPrintf(
      "MusicSection(name=\"%s\", title=\"%s\", "
      "music_recommendations=[%s])",
      name_.c_str(), title_.c_str(), s.c_str());
}

TopLevelMusicRecommendation::TopLevelMusicRecommendation() = default;
TopLevelMusicRecommendation::~TopLevelMusicRecommendation() = default;

// static
void TopLevelMusicRecommendation::RegisterJSONConverter(
    JSONValueConverter<TopLevelMusicRecommendation>* converter) {
  converter->RegisterNestedField(kApiResponseMusicSectionKey,
                                 &TopLevelMusicRecommendation::music_section_);
}

// static
std::unique_ptr<TopLevelMusicRecommendation>
TopLevelMusicRecommendation::CreateFrom(const base::Value& value) {
  auto top_level_music_recommendation =
      std::make_unique<TopLevelMusicRecommendation>();
  JSONValueConverter<TopLevelMusicRecommendation> converter;
  if (!converter.Convert(value, top_level_music_recommendation.get())) {
    DVLOG(1) << "Unable to construct `TopLevelMusicRecommendation` from parsed "
                "json.";
    return nullptr;
  }
  return top_level_music_recommendation;
}

std::string TopLevelMusicRecommendation::ToString() const {
  return base::StringPrintf("TopLevelMusicRecommendation(music_section=%s)",
                            music_section_.ToString().c_str());
}

TopLevelMusicRecommendations::TopLevelMusicRecommendations() = default;
TopLevelMusicRecommendations::~TopLevelMusicRecommendations() = default;

// static
void TopLevelMusicRecommendations::RegisterJSONConverter(
    JSONValueConverter<TopLevelMusicRecommendations>* converter) {
  converter->RegisterRepeatedMessage<TopLevelMusicRecommendation>(
      kApiResponseMusicRecommendationsKey,
      &TopLevelMusicRecommendations::top_level_music_recommendations_);
}

// static
std::unique_ptr<TopLevelMusicRecommendations>
TopLevelMusicRecommendations::CreateFrom(const base::Value& value) {
  auto top_level_music_recommendations =
      std::make_unique<TopLevelMusicRecommendations>();
  JSONValueConverter<TopLevelMusicRecommendations> converter;
  if (!converter.Convert(value, top_level_music_recommendations.get())) {
    DVLOG(1) << "Unable to construct `TopLevelMusicRecommendations` from "
                "parsed json.";
    return nullptr;
  }
  return top_level_music_recommendations;
}

std::string TopLevelMusicRecommendations::ToString() const {
  std::string s;
  for (size_t i = 0; i < top_level_music_recommendations_.size(); i++) {
    s += (i ? ", " : "") + top_level_music_recommendations_[i]->ToString();
  }
  return base::StringPrintf(
      "TopLevelMusicRecommendations(top_level_music_recommendations=[%s])",
      s.c_str());
}

ArtistReference::ArtistReference() = default;
ArtistReference::~ArtistReference() = default;

// static
void ArtistReference::RegisterJSONConverter(
    JSONValueConverter<ArtistReference>* converter) {
  converter->RegisterStringField(kApiResponseArtistKey,
                                 &ArtistReference::artist_);
  converter->RegisterStringField(kApiResponseTitleKey,
                                 &ArtistReference::title_);
}

// static
std::unique_ptr<ArtistReference> ArtistReference::CreateFrom(
    const base::Value& value) {
  auto artist_reference = std::make_unique<ArtistReference>();
  JSONValueConverter<ArtistReference> converter;
  if (!converter.Convert(value, artist_reference.get())) {
    DVLOG(1) << "Unable to construct `ArtistReference` from parsed json.";
    return nullptr;
  }
  return artist_reference;
}

std::string ArtistReference::ToString() const {
  return base::StringPrintf("ArtistReference(artist=\"%s\", title=\"%s\"",
                            artist_.c_str(), title_.c_str());
}

Track::Track() = default;
Track::~Track() = default;

// static
void Track::RegisterJSONConverter(JSONValueConverter<Track>* converter) {
  converter->RegisterStringField(kApiResponseNameKey, &Track::name_);
  converter->RegisterStringField(kApiResponseTitleKey, &Track::title_);
  converter->RegisterStringField(kApiResponseDurationKey, &Track::duration_);
  converter->RegisterStringField(kApiResponseExplicitTypeKey,
                                 &Track::explicit_type_);
  converter->RegisterRepeatedMessage<Image>(kApiResponseImagesKey,
                                            &Track::images_);
  converter->RegisterRepeatedMessage<ArtistReference>(
      kApiResponseArtistReferencesKey, &Track::artist_references_);
}

// static
std::unique_ptr<Track> Track::CreateFrom(const base::Value& value) {
  auto track = std::make_unique<Track>();
  JSONValueConverter<Track> converter;
  if (!converter.Convert(value, track.get())) {
    DVLOG(1) << "Unable to construct `Track` from parsed json.";
    return nullptr;
  }
  return track;
}

std::string Track::ToString() const {
  std::string s;
  for (size_t i = 0; i < images_.size(); i++) {
    s += (i ? ", " : "") + images_[i]->ToString();
  }
  return base::StringPrintf(
      "Track(name=\"%s\", title=\"%s\", duration=\"%s\", explicit_type_=%s, "
      "images=[%s])",
      name_.c_str(), title_.c_str(), duration_.c_str(), explicit_type_.c_str(),
      s.c_str());
}

QueueItem::QueueItem() = default;
QueueItem::~QueueItem() = default;

// static
void QueueItem::RegisterJSONConverter(
    JSONValueConverter<QueueItem>* converter) {
  converter->RegisterNestedField(kApiResponseTrackKey, &QueueItem::track_);
}

// static
std::unique_ptr<QueueItem> QueueItem::CreateFrom(const base::Value& value) {
  auto queue_item = std::make_unique<QueueItem>();
  JSONValueConverter<QueueItem> converter;
  if (!converter.Convert(value, queue_item.get())) {
    DVLOG(1) << "Unable to construct `QueueItem` from parsed json.";
    return nullptr;
  }
  return queue_item;
}

std::string QueueItem::ToString() const {
  return base::StringPrintf("QueueItem(track=%s)", track_.ToString().c_str());
}

Stream::Stream() = default;
Stream::~Stream() = default;

// static
void Stream::RegisterJSONConverter(JSONValueConverter<Stream>* converter) {
  converter->RegisterCustomField<GURL>(kApiResponseUriKey, &Stream::url_,
                                       &ConvertToGurl);
  converter->RegisterStringField(kApiResponsePlaybackReportingTokenKey,
                                 &Stream::playback_reporting_token_);
}

// static
std::unique_ptr<Stream> Stream::CreateFrom(const base::Value& value) {
  auto stream = std::make_unique<Stream>();
  JSONValueConverter<Stream> converter;
  if (!converter.Convert(value, stream.get())) {
    DVLOG(1) << "Unable to construct `Stream` from parsed json.";
    return nullptr;
  }
  return stream;
}

std::string Stream::ToString() const {
  return base::StringPrintf(
      "Stream(url=\"%s\", playback_reporting_token=\"%s\")",
      url_.spec().c_str(), playback_reporting_token_.c_str());
}

PlaybackManifest::PlaybackManifest() = default;
PlaybackManifest::~PlaybackManifest() = default;

// static
void PlaybackManifest::RegisterJSONConverter(
    JSONValueConverter<PlaybackManifest>* converter) {
  converter->RegisterRepeatedMessage<Stream>(kApiResponseStreamsKey,
                                             &PlaybackManifest::streams_);
}

// static
std::unique_ptr<PlaybackManifest> PlaybackManifest::CreateFrom(
    const base::Value& value) {
  auto playback_manifest = std::make_unique<PlaybackManifest>();
  JSONValueConverter<PlaybackManifest> converter;
  if (!converter.Convert(value, playback_manifest.get())) {
    DVLOG(1) << "Unable to construct `PlaybackManifest` from parsed json.";
    return nullptr;
  }
  return playback_manifest;
}

std::string PlaybackManifest::ToString() const {
  std::string s;
  for (size_t i = 0; i < streams_.size(); i++) {
    s += (i ? ", " : "") + streams_[i]->ToString();
  }
  return base::StringPrintf("PlaybackManifest(streams=[%s])", s.c_str());
}

PlaybackContext::PlaybackContext() = default;
PlaybackContext::~PlaybackContext() = default;

// static
void PlaybackContext::RegisterJSONConverter(
    JSONValueConverter<PlaybackContext>* converter) {
  converter->RegisterNestedField(kApiResponseItemKey,
                                 &PlaybackContext::queue_item_);
  converter->RegisterNestedField(kApiResponseManifestKey,
                                 &PlaybackContext::playback_manifest_);
}

// static
std::unique_ptr<PlaybackContext> PlaybackContext::CreateFrom(
    const base::Value& value) {
  auto playback_context = std::make_unique<PlaybackContext>();
  JSONValueConverter<PlaybackContext> converter;
  if (!converter.Convert(value, playback_context.get())) {
    DVLOG(1) << "Unable to construct `PlaybackManifest` from parsed json.";
    return nullptr;
  }
  return playback_context;
}

std::string PlaybackContext::ToString() const {
  return base::StringPrintf(
      "PlaybackContext(queue_item=%s, playback_manifest=%s)",
      queue_item_.ToString().c_str(), playback_manifest_.ToString().c_str());
}

Queue::Queue() = default;
Queue::~Queue() = default;

// static
void Queue::RegisterJSONConverter(JSONValueConverter<Queue>* converter) {
  converter->RegisterStringField(kApiResponseNameKey, &Queue::name_);
  converter->RegisterIntField(kApiResponseSizeKey, &Queue::size_);
  converter->RegisterNestedField(kApiResponseContextKey,
                                 &Queue::playback_context_);
}

// static
std::unique_ptr<Queue> Queue::CreateFrom(const base::Value& value) {
  auto queue = std::make_unique<Queue>();
  JSONValueConverter<Queue> converter;
  if (!converter.Convert(value, queue.get())) {
    DVLOG(1) << "Unable to construct `Queue` from parsed json.";
    return nullptr;
  }
  return queue;
}

std::string Queue::ToString() const {
  return base::StringPrintf("Queue(name=\"%s\", size=%d, playback_context=%s)",
                            name_.c_str(), size_,
                            playback_context_.ToString().c_str());
}

QueueContainer::QueueContainer() = default;
QueueContainer::~QueueContainer() = default;

// static
void QueueContainer::RegisterJSONConverter(
    JSONValueConverter<QueueContainer>* converter) {
  converter->RegisterNestedField(kApiResponseQueueKey, &QueueContainer::queue_);
}

// static
std::unique_ptr<QueueContainer> QueueContainer::CreateFrom(
    const base::Value& value) {
  auto queue_container = std::make_unique<QueueContainer>();
  JSONValueConverter<QueueContainer> converter;
  if (!converter.Convert(value, queue_container.get())) {
    DVLOG(1) << "Unable to construct `QueueContainer` from parsed json.";
    return nullptr;
  }
  return queue_container;
}

std::string QueueContainer::ToString() const {
  return base::StringPrintf("QueueContainer(queue=%s)",
                            queue_.ToString().c_str());
}

ReportPlaybackResult::ReportPlaybackResult() = default;
ReportPlaybackResult::~ReportPlaybackResult() = default;

// static
void ReportPlaybackResult::RegisterJSONConverter(
    JSONValueConverter<ReportPlaybackResult>* converter) {
  converter->RegisterStringField(
      kApiResponsePlaybackReportingTokenKey,
      &ReportPlaybackResult::playback_reporting_token_);
}

// static
std::unique_ptr<ReportPlaybackResult> ReportPlaybackResult::CreateFrom(
    const base::Value& value) {
  auto report_playback_result = std::make_unique<ReportPlaybackResult>();
  JSONValueConverter<ReportPlaybackResult> converter;
  if (!converter.Convert(value, report_playback_result.get())) {
    DVLOG(1) << "Unable to construct `ReportPlaybackResult` from parsed json.";
    return nullptr;
  }
  return report_playback_result;
}

std::string ReportPlaybackResult::ToString() const {
  return base::StringPrintf("ReportPlaybackResult(playback_reporting_token=%s)",
                            playback_reporting_token_.c_str());
}

}  // namespace google_apis::youtube_music
