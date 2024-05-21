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

GetPlaylistsRequest::GetPlaylistsRequest(RequestSender* sender,
                                         Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)) {
  CHECK(!callback_.is_null());
}

GetPlaylistsRequest::~GetPlaylistsRequest() = default;

GURL GetPlaylistsRequest::GetURL() const {
  // TODO(b/341324009): Move to an util file or class.
  return GURL(
      "https://youtubemediaconnect.googleapis.com/v1/musicSections/"
      "root:load?intent=focus&category=music&sectionRecommendationLimit=10");
}

ApiErrorCode GetPlaylistsRequest::MapReasonToError(ApiErrorCode code,
                                                   const std::string& reason) {
  return code;
}

bool GetPlaylistsRequest::IsSuccessfulErrorCode(ApiErrorCode error) {
  return error == HTTP_SUCCESS;
}

void GetPlaylistsRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  switch (error) {
    case HTTP_SUCCESS:
      blocking_task_runner()->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&GetPlaylistsRequest::Parse, std::move(response_body)),
          base::BindOnce(&GetPlaylistsRequest::OnDataParsed,
                         weak_ptr_factory_.GetWeakPtr()));
      break;
    default:
      RunCallbackOnPrematureFailure(error);
      OnProcessURLFetchResultsComplete();
      break;
  }
}

void GetPlaylistsRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  std::move(callback_).Run(base::unexpected(error));
}

std::unique_ptr<TopLevelMusicRecommendations> GetPlaylistsRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? TopLevelMusicRecommendations::CreateFrom(*value) : nullptr;
}

void GetPlaylistsRequest::OnDataParsed(
    std::unique_ptr<TopLevelMusicRecommendations> recommendations) {
  if (!recommendations) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(recommendations));
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

void PlaybackQueuePrepareRequest::OnDataParsed(
    std::unique_ptr<Queue> playback_queue) {
  if (!playback_queue) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(playback_queue));
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
    std::unique_ptr<QueueContainer> playback_queue) {
  if (!playback_queue) {
    std::move(callback_).Run(base::unexpected(PARSE_ERROR));
  } else {
    std::move(callback_).Run(std::move(playback_queue));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::youtube_music
