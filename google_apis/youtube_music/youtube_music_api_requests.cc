// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/youtube_music/youtube_music_api_requests.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "google_apis/youtube_music/youtube_music_api_response_types.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

constexpr char kContentTypeJson[] = "application/json; charset=utf-8";

}  // namespace

namespace google_apis::youtube_music {

GetMusicSectionRequest::GetMusicSectionRequest(RequestSender* sender,
                                               Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

GetMusicSectionRequest::~GetMusicSectionRequest() = default;

GURL GetMusicSectionRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  return GURL(
      "https://youtubemediaconnect.googleapis.com/v1/musicSections/"
      "root:load?intent=focus&category=music&sectionRecommendationLimit=10");
}

ApiErrorCode GetMusicSectionRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool GetMusicSectionRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void GetMusicSectionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetMusicSectionRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&GetMusicSectionRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetMusicSectionRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<TopLevelMusicRecommendations> GetMusicSectionRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? TopLevelMusicRecommendations::CreateFrom(*value) : nullptr;
}

void GetMusicSectionRequest::OnDataParsed(
    std::unique_ptr<TopLevelMusicRecommendations> recommendations) {
  if (!recommendations) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(recommendations));
  }
  OnProcessURLFetchResultsComplete();
}

GetPlaylistRequest::GetPlaylistRequest(RequestSender* sender,
                                       const std::string& playlist_name,
                                       Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      playlist_name_(playlist_name),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

GetPlaylistRequest::~GetPlaylistRequest() = default;

GURL GetPlaylistRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  return GURL(
      base::StringPrintf("https://youtubemediaconnect.googleapis.com/v1/%s",
                         playlist_name_.c_str()));
}

ApiErrorCode GetPlaylistRequest::MapReasonToError(ApiErrorCode code,
                                                  const std::string& reason) {
  return code;
}

bool GetPlaylistRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void GetPlaylistRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();

  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetPlaylistRequest::Parse, std::move(response_body)),
          base::BindOnce(&GetPlaylistRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetPlaylistRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<Playlist> GetPlaylistRequest::Parse(const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Playlist::CreateFrom(*value) : nullptr;
}

void GetPlaylistRequest::OnDataParsed(std::unique_ptr<Playlist> playlist) {
  if (!playlist) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(playlist));
  }
  OnProcessURLFetchResultsComplete();
}

PlaybackQueuePrepareRequest::PlaybackQueuePrepareRequest(
    RequestSender* sender,
    const PlaybackQueuePrepareRequestPayload& payload,
    Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      payload_(payload),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

PlaybackQueuePrepareRequest::~PlaybackQueuePrepareRequest() = default;

GURL PlaybackQueuePrepareRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  GURL url(
      "https://youtubemediaconnect.googleapis.com/v1/queues/"
      "default:preparePlayback");
  return url;
}

ApiErrorCode PlaybackQueuePrepareRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool PlaybackQueuePrepareRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

HttpRequestMethod PlaybackQueuePrepareRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool PlaybackQueuePrepareRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = kContentTypeJson;
  *upload_content = payload_.ToJson();
  return true;
}

void PlaybackQueuePrepareRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PlaybackQueuePrepareRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&PlaybackQueuePrepareRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void PlaybackQueuePrepareRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<Queue> PlaybackQueuePrepareRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Queue::CreateFrom(*value) : nullptr;
}

void PlaybackQueuePrepareRequest::OnDataParsed(std::unique_ptr<Queue> queue) {
  if (!queue) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(queue));
  }
  OnProcessURLFetchResultsComplete();
}

PlaybackQueueNextRequest::PlaybackQueueNextRequest(
    RequestSender* sender,
    Callback callback,
    const std::string& playback_queue_name)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
  playback_queue_name_ = playback_queue_name;
}

PlaybackQueueNextRequest::~PlaybackQueueNextRequest() = default;

GURL PlaybackQueueNextRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  return GURL(base::StringPrintf(
      "https://youtubemediaconnect.googleapis.com/v1/%s:next",
      playback_queue_name_.c_str()));
}

ApiErrorCode PlaybackQueueNextRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool PlaybackQueueNextRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

HttpRequestMethod PlaybackQueueNextRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

void PlaybackQueueNextRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&PlaybackQueueNextRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&PlaybackQueueNextRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void PlaybackQueueNextRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<QueueContainer> PlaybackQueueNextRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? QueueContainer::CreateFrom(*value) : nullptr;
}

void PlaybackQueueNextRequest::OnDataParsed(
    std::unique_ptr<QueueContainer> queue_container) {
  if (!queue_container) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(queue_container));
  }
  OnProcessURLFetchResultsComplete();
}

ReportPlaybackRequest::ReportPlaybackRequest(
    RequestSender* sender,
    std::unique_ptr<ReportPlaybackRequestPayload> payload,
    Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      payload_(std::move(payload)),
      callback_(std::move(callback)) {
  CHECK(payload_);
  CHECK(!callback_.is_null());
}

ReportPlaybackRequest::~ReportPlaybackRequest() = default;

GURL ReportPlaybackRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  return GURL("https://youtubemediaconnect.googleapis.com/v1/reports/playback");
}

ApiErrorCode ReportPlaybackRequest::MapReasonToError(
    ApiErrorCode code,
    const std::string& reason) {
  return code;
}

bool ReportPlaybackRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

HttpRequestMethod ReportPlaybackRequest::GetRequestType() const {
  return HttpRequestMethod::kPost;
}

bool ReportPlaybackRequest::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = kContentTypeJson;
  *upload_content = payload_->ToJson();
  return true;
}

void ReportPlaybackRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ReportPlaybackRequest::Parse,
                         std::move(response_body)),
          base::BindOnce(&ReportPlaybackRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void ReportPlaybackRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<ReportPlaybackResult> ReportPlaybackRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? ReportPlaybackResult::CreateFrom(*value) : nullptr;
}

void ReportPlaybackRequest::OnDataParsed(
    std::unique_ptr<ReportPlaybackResult> report_playback_result) {
  if (!report_playback_result) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(report_playback_result));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::youtube_music
