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

// Returns true if a localized error message is expected in the response body
// for a response with error code `error`.
bool ErrorMessageExpected(google_apis::ApiErrorCode error) {
  switch (error) {
    case google_apis::HTTP_BAD_REQUEST:
    case google_apis::HTTP_UNAUTHORIZED:
    case google_apis::HTTP_FORBIDDEN:
    case google_apis::HTTP_NOT_FOUND:
    case google_apis::HTTP_CONFLICT:
    case google_apis::HTTP_GONE:
    case google_apis::HTTP_LENGTH_REQUIRED:
    case google_apis::HTTP_PRECONDITION:
    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
    case google_apis::HTTP_NOT_IMPLEMENTED:
    case google_apis::HTTP_BAD_GATEWAY:
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
      return true;
    default:
      return false;
  }
}

template <class T>
void RunCallbackWithError(T callback, google_apis::ApiErrorCode error) {
  std::move(callback).Run(base::unexpected(google_apis::youtube_music::ApiError{
      .error_code = error, .error_message = std::string()}));
}

template <class T>
void RunErrorCallback(
    google_apis::ApiErrorCode error_code,
    base::OnceCallback<
        void(base::expected<T, google_apis::youtube_music::ApiError>)> callback,
    base::OnceClosure on_done,
    std::string error_message) {
  google_apis::youtube_music::ApiError error{
      .error_code = error_code, .error_message = std::move(error_message)};
  std::move(callback).Run(base::unexpected(std::move(error)));
  std::move(on_done).Run();
}

// Attempts to parse `response_body` for the localized error message and uses it
// to populate the ApiError in `callback`. When complete, runs `finish_request`.
// Parses the `response_body` on `task_runner`.
template <class T>
void ParseErrorAsync(
    base::SequencedTaskRunner* task_runner,
    google_apis::ApiErrorCode error,
    std::string response_body,
    base::OnceCallback<
        void(base::expected<T, google_apis::youtube_music::ApiError>)> callback,
    base::OnceClosure finish_request) {
  task_runner->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(google_apis::youtube_music::ParseErrorJson,
                     std::move(response_body)),
      base::BindOnce(RunErrorCallback<T>, error, std::move(callback),
                     std::move(finish_request)));
}

template <class T>
void HandleError(
    base::SequencedTaskRunner* task_runner,
    google_apis::ApiErrorCode error,
    std::string&& response_body,
    base::OnceCallback<
        void(base::expected<T, google_apis::youtube_music::ApiError>)> callback,
    base::OnceClosure finish_request) {
  if (ErrorMessageExpected(error)) {
    ParseErrorAsync(task_runner, error, std::move(response_body),
                    std::move(callback), std::move(finish_request));
    return;
  }

  RunCallbackWithError(std::move(callback), error);
  std::move(finish_request).Run();
}

}  // namespace

namespace google_apis::youtube_music {

GetMusicSectionRequest::GetMusicSectionRequest(RequestSender* sender,
                                               const std::string& device_info,
                                               Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      callback_(std::move(callback)),
      device_info_(device_info) {
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

std::vector<std::string> GetMusicSectionRequest::GetExtraRequestHeaders()
    const {
  return {device_info_};
}

void GetMusicSectionRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  if (error == HTTP_SUCCESS) {
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetMusicSectionRequest::Parse,
                       std::move(response_body)),
        base::BindOnce(&GetMusicSectionRequest::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::OnceClosure finish_request =
      base::BindOnce(&GetMusicSectionRequest::OnProcessURLFetchResultsComplete,
                     weak_ptr_factory_.GetWeakPtr());
  HandleError(blocking_task_runner(), error, std::move(response_body),
              std::move(callback_), std::move(finish_request));
}

void GetMusicSectionRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  RunCallbackWithError(std::move(callback_), error);
}

std::unique_ptr<TopLevelMusicRecommendations> GetMusicSectionRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? TopLevelMusicRecommendations::CreateFrom(*value) : nullptr;
}

void GetMusicSectionRequest::OnDataParsed(
    std::unique_ptr<TopLevelMusicRecommendations> recommendations) {
  if (!recommendations) {
    RunCallbackWithError(std::move(callback_), PARSE_ERROR);
  } else {
    std::move(callback_).Run(std::move(recommendations));
  }
  OnProcessURLFetchResultsComplete();
}

GetPlaylistRequest::GetPlaylistRequest(RequestSender* sender,
                                       const std::string& device_info,
                                       const std::string& playlist_name,
                                       Callback callback)
    : UrlFetchRequestBase(sender, ProgressCallback(), ProgressCallback()),
      device_info_(device_info),
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

std::vector<std::string> GetPlaylistRequest::GetExtraRequestHeaders() const {
  return {device_info_};
}

void GetPlaylistRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  if (error == HTTP_SUCCESS) {
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&GetPlaylistRequest::Parse, std::move(response_body)),
        base::BindOnce(&GetPlaylistRequest::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::OnceClosure finish_request =
      base::BindOnce(&GetPlaylistRequest::OnProcessURLFetchResultsComplete,
                     weak_ptr_factory_.GetWeakPtr());
  HandleError(blocking_task_runner(), error, std::move(response_body),
              std::move(callback_), std::move(finish_request));
}

void GetPlaylistRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  RunCallbackWithError(std::move(callback_), error);
}

std::unique_ptr<Playlist> GetPlaylistRequest::Parse(const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Playlist::CreateFrom(*value) : nullptr;
}

void GetPlaylistRequest::OnDataParsed(std::unique_ptr<Playlist> playlist) {
  if (!playlist) {
    RunCallbackWithError(std::move(callback_), PARSE_ERROR);
  } else {
    std::move(callback_).Run(std::move(playlist));
  }
  OnProcessURLFetchResultsComplete();
}

PlaybackQueuePrepareRequest::PlaybackQueuePrepareRequest(
    RequestSender* sender,
    const PlaybackQueuePrepareRequestPayload& payload,
    Callback callback)
    : SignedRequest(sender), payload_(payload), callback_(std::move(callback)) {
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
  if (error == HTTP_SUCCESS) {
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&PlaybackQueuePrepareRequest::Parse,
                       std::move(response_body)),
        base::BindOnce(&PlaybackQueuePrepareRequest::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::OnceClosure finish_request = base::BindOnce(
      &PlaybackQueuePrepareRequest::OnProcessURLFetchResultsComplete,
      weak_ptr_factory_.GetWeakPtr());
  HandleError(blocking_task_runner(), error, std::move(response_body),
              std::move(callback_), std::move(finish_request));
}

void PlaybackQueuePrepareRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  RunCallbackWithError(std::move(callback_), error);
}

std::unique_ptr<Queue> PlaybackQueuePrepareRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? Queue::CreateFrom(*value) : nullptr;
}

void PlaybackQueuePrepareRequest::OnDataParsed(std::unique_ptr<Queue> queue) {
  if (!queue) {
    RunCallbackWithError(std::move(callback_), PARSE_ERROR);
  } else {
    std::move(callback_).Run(std::move(queue));
  }
  OnProcessURLFetchResultsComplete();
}

PlaybackQueueNextRequest::PlaybackQueueNextRequest(
    RequestSender* sender,
    const PlaybackQueueNextRequestPayload& payload,
    Callback callback,
    const std::string& playback_queue_name)
    : SignedRequest(sender), payload_(payload), callback_(std::move(callback)) {
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

bool PlaybackQueueNextRequest::GetContentData(std::string* upload_content_type,
                                              std::string* upload_content) {
  *upload_content_type = kContentTypeJson;
  *upload_content = payload_.ToJson();
  return true;
}

void PlaybackQueueNextRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  ApiErrorCode error = GetErrorCode();
  if (error == HTTP_SUCCESS) {
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&PlaybackQueueNextRequest::Parse,
                       std::move(response_body)),
        base::BindOnce(&PlaybackQueueNextRequest::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::OnceClosure finish_request = base::BindOnce(
      &PlaybackQueueNextRequest::OnProcessURLFetchResultsComplete,
      weak_ptr_factory_.GetWeakPtr());
  HandleError(blocking_task_runner(), error, std::move(response_body),
              std::move(callback_), std::move(finish_request));
}

void PlaybackQueueNextRequest::RunCallbackOnPrematureFailure(
    ApiErrorCode error) {
  RunCallbackWithError(std::move(callback_), error);
}

std::unique_ptr<QueueContainer> PlaybackQueueNextRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? QueueContainer::CreateFrom(*value) : nullptr;
}

void PlaybackQueueNextRequest::OnDataParsed(
    std::unique_ptr<QueueContainer> queue_container) {
  if (!queue_container) {
    RunCallbackWithError(std::move(callback_), PARSE_ERROR);
  } else {
    std::move(callback_).Run(std::move(queue_container));
  }
  OnProcessURLFetchResultsComplete();
}

ReportPlaybackRequest::ReportPlaybackRequest(
    RequestSender* sender,
    std::unique_ptr<ReportPlaybackRequestPayload> payload,
    Callback callback)
    : SignedRequest(sender),
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
  if (error == HTTP_SUCCESS) {
    blocking_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&ReportPlaybackRequest::Parse, std::move(response_body)),
        base::BindOnce(&ReportPlaybackRequest::OnDataParsed,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  base::OnceClosure finish_request =
      base::BindOnce(&ReportPlaybackRequest::OnProcessURLFetchResultsComplete,
                     weak_ptr_factory_.GetWeakPtr());
  HandleError(blocking_task_runner(), error, std::move(response_body),
              std::move(callback_), std::move(finish_request));
}

void ReportPlaybackRequest::RunCallbackOnPrematureFailure(ApiErrorCode error) {
  RunCallbackWithError(std::move(callback_), error);
}

std::unique_ptr<ReportPlaybackResult> ReportPlaybackRequest::Parse(
    const std::string& json) {
  std::unique_ptr<base::Value> value = ParseJson(json);
  return value ? ReportPlaybackResult::CreateFrom(*value) : nullptr;
}

void ReportPlaybackRequest::OnDataParsed(
    std::unique_ptr<ReportPlaybackResult> report_playback_result) {
  if (!report_playback_result) {
    RunCallbackWithError(std::move(callback_), PARSE_ERROR);
  } else {
    std::move(callback_).Run(std::move(report_playback_result));
  }
  OnProcessURLFetchResultsComplete();
}

}  // namespace google_apis::youtube_music
