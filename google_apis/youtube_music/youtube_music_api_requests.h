// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUESTS_H_
#define GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUESTS_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/youtube_music/youtube_music_api_request_types.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"

namespace google_apis {

enum ApiErrorCode;
class RequestSender;

namespace youtube_music {

// Request that gets music sections from the API server. For now this always
// sets the intent to "focus". For API usage, please check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/musicSections/load
class GetMusicSectionRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<TopLevelMusicRecommendations>, ApiError>)>;

  GetMusicSectionRequest(RequestSender* sender,
                         const std::string& device_info,
                         Callback callback);
  GetMusicSectionRequest(const GetMusicSectionRequest&) = delete;
  GetMusicSectionRequest& operator=(const GetMusicSectionRequest&) = delete;
  ~GetMusicSectionRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<TopLevelMusicRecommendations> Parse(
      const std::string& json);

  void OnDataParsed(
      std::unique_ptr<TopLevelMusicRecommendations> recommendations);

  Callback callback_;
  const std::string device_info_;

  base::WeakPtrFactory<GetMusicSectionRequest> weak_ptr_factory_{this};
};

// Request that gets playlist with name `playlist_name` from the API server. For
// API usage, please check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/playlists/get
class GetPlaylistRequest : public UrlFetchRequestBase {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Playlist>, ApiError>)>;

  GetPlaylistRequest(RequestSender* sender,
                     const std::string& device_info,
                     const std::string& playlist_name,
                     Callback callback);
  GetPlaylistRequest(const GetPlaylistRequest&) = delete;
  GetPlaylistRequest& operator=(const GetPlaylistRequest&) = delete;
  ~GetPlaylistRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  std::vector<std::string> GetExtraRequestHeaders() const override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<Playlist> Parse(const std::string& json);

  void OnDataParsed(std::unique_ptr<Playlist> playlist);

  const std::string device_info_;

  // Playlist name. Unique identifier of a playlist.
  std::string playlist_name_;

  Callback callback_;

  base::WeakPtrFactory<GetPlaylistRequest> weak_ptr_factory_{this};
};

// Request that prepares the playback queue for the API server. For API usage,
// please check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues/preparePlayback
class PlaybackQueuePrepareRequest : public SignedRequest {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<Queue>, ApiError>)>;

  PlaybackQueuePrepareRequest(RequestSender* sender,
                              const PlaybackQueuePrepareRequestPayload& payload,
                              Callback callback);
  PlaybackQueuePrepareRequest(const PlaybackQueuePrepareRequest&) = delete;
  PlaybackQueuePrepareRequest& operator=(const PlaybackQueuePrepareRequest&) =
      delete;
  ~PlaybackQueuePrepareRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<Queue> Parse(const std::string& json);

  void OnDataParsed(std::unique_ptr<Queue> queue);

  const PlaybackQueuePrepareRequestPayload payload_;

  Callback callback_;

  base::WeakPtrFactory<PlaybackQueuePrepareRequest> weak_ptr_factory_{this};
};

// Request to play the next in the playback queue for the API server. For API
// usage, please check below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/queues/next
class PlaybackQueueNextRequest : public SignedRequest {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<QueueContainer>, ApiError>)>;

  PlaybackQueueNextRequest(RequestSender* sender,
                           const PlaybackQueueNextRequestPayload& payload,
                           Callback callback,
                           const std::string& playback_queue_name);
  PlaybackQueueNextRequest(const PlaybackQueueNextRequest&) = delete;
  PlaybackQueueNextRequest& operator=(const PlaybackQueueNextRequest&) = delete;
  ~PlaybackQueueNextRequest() override;

  const std::string& playback_queue_name() const {
    return playback_queue_name_;
  }
  void set_playback_queue_name(const std::string& playback_queue_name) {
    playback_queue_name_ = playback_queue_name;
  }

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<QueueContainer> Parse(const std::string& json);

  void OnDataParsed(std::unique_ptr<QueueContainer> queue_container);

  const PlaybackQueueNextRequestPayload payload_;

  // Playlist queue name. Unique identifier of a queue.
  std::string playback_queue_name_;

  Callback callback_;

  base::WeakPtrFactory<PlaybackQueueNextRequest> weak_ptr_factory_{this};
};

// Request to report the playback to the API server. For API usage, please check
// below:
//   https://developers.google.com/youtube/mediaconnect/reference/rest/v1/reports/playback
class ReportPlaybackRequest : public SignedRequest {
 public:
  using Callback = base::OnceCallback<void(
      base::expected<std::unique_ptr<ReportPlaybackResult>, ApiError>)>;

  ReportPlaybackRequest(RequestSender* sender,
                        std::unique_ptr<ReportPlaybackRequestPayload> payload,
                        Callback callback);
  ReportPlaybackRequest(const ReportPlaybackRequest&) = delete;
  ReportPlaybackRequest& operator=(const ReportPlaybackRequest&) = delete;
  ~ReportPlaybackRequest() override;

 protected:
  // UrlFetchRequestBase:
  GURL GetURL() const override;
  ApiErrorCode MapReasonToError(ApiErrorCode code,
                                const std::string& reason) override;
  bool IsSuccessfulErrorCode(ApiErrorCode error) override;
  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override;
  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      const base::FilePath response_file,
      std::string response_body) override;
  void RunCallbackOnPrematureFailure(ApiErrorCode code) override;

 private:
  static std::unique_ptr<ReportPlaybackResult> Parse(const std::string& json);

  void OnDataParsed(
      std::unique_ptr<ReportPlaybackResult> report_playback_result);

  const std::unique_ptr<ReportPlaybackRequestPayload> payload_;

  Callback callback_;

  base::WeakPtrFactory<ReportPlaybackRequest> weak_ptr_factory_{this};
};

}  // namespace youtube_music
}  // namespace google_apis

#endif  // GOOGLE_APIS_YOUTUBE_MUSIC_YOUTUBE_MUSIC_API_REQUESTS_H_
