// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/mock_background_fetch_delegate.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/background_fetch_description.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/header_util.h"

namespace content {

MockBackgroundFetchDelegate::TestResponse::TestResponse() = default;

MockBackgroundFetchDelegate::TestResponse::~TestResponse() = default;

MockBackgroundFetchDelegate::TestResponseBuilder::TestResponseBuilder(
    int response_code)
    : response_(std::make_unique<TestResponse>()) {
  response_->succeeded = network::IsSuccessfulStatus(response_code);
  response_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      "HTTP/1.1 " + base::NumberToString(response_code));
}

MockBackgroundFetchDelegate::TestResponseBuilder::~TestResponseBuilder() =
    default;

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::AddResponseHeader(
    const std::string& name,
    const std::string& value) {
  DCHECK(response_);
  response_->headers->AddHeader(name, value);
  return *this;
}

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::SetResponseData(
    std::string data) {
  DCHECK(response_);
  response_->data.swap(data);
  return *this;
}

MockBackgroundFetchDelegate::TestResponseBuilder&
MockBackgroundFetchDelegate::TestResponseBuilder::MakeIndefinitelyPending() {
  response_->pending = true;
  return *this;
}

std::unique_ptr<MockBackgroundFetchDelegate::TestResponse>
MockBackgroundFetchDelegate::TestResponseBuilder::Build() {
  return std::move(response_);
}

MockBackgroundFetchDelegate::MockBackgroundFetchDelegate() {}

MockBackgroundFetchDelegate::~MockBackgroundFetchDelegate() {}

void MockBackgroundFetchDelegate::GetIconDisplaySize(
    GetIconDisplaySizeCallback callback) {}

void MockBackgroundFetchDelegate::CreateDownloadJob(
    base::WeakPtr<Client> client,
    std::unique_ptr<BackgroundFetchDescription> fetch_description) {
  job_id_to_client_map_[fetch_description->job_unique_id] = std::move(client);
}

void MockBackgroundFetchDelegate::DownloadUrl(
    const std::string& job_unique_id,
    const std::string& guid,
    const std::string& method,
    const GURL& url,
    ::network::mojom::CredentialsMode credentials_mode,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers,
    bool has_request_body) {
  DCHECK(!seen_guids_.count(guid));

  download_guid_to_job_id_map_[guid] = job_unique_id;

  auto url_iter = url_responses_.find(url);
  if (url_iter == url_responses_.end()) {
    // Since no response was provided, do not respond. This allows testing
    // long-lived fetches.
    return;
  }

  std::unique_ptr<TestResponse> test_response = std::move(url_iter->second);
  url_responses_.erase(url_iter);

  if (test_response->pending)
    return;

  PostAbortCheckingTask(
      job_unique_id,
      base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadStarted,
                     job_id_to_client_map_[job_unique_id], job_unique_id, guid,
                     std::make_unique<BackgroundFetchResponse>(
                         std::vector<GURL>({url}), test_response->headers)));

  uint64_t bytes_uploaded = 0u;

  if (has_request_body) {
    // This value isn't actually used anywhere within content tests so a random
    // value is OK.
    bytes_uploaded = 42u;

    // Report upload progress.
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadUpdated,
                       job_id_to_client_map_[job_unique_id], job_unique_id,
                       guid, /* bytes_download= */ bytes_uploaded, 0u));
  }

  if (test_response->data.size()) {
    // Report progress at 50% complete.
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadUpdated,
                       job_id_to_client_map_[job_unique_id], job_unique_id,
                       guid, bytes_uploaded, test_response->data.size() / 2));

    // Report progress at 100% complete.
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadUpdated,
                       job_id_to_client_map_[job_unique_id], job_unique_id,
                       guid, bytes_uploaded, test_response->data.size()));
  }

  if (test_response->succeeded) {
    base::FilePath response_path;
    if (!temp_directory_.IsValid()) {
      CHECK(temp_directory_.CreateUniqueTempDir());
    }

    // Write the |response|'s data to a temporary file.
    CHECK(base::CreateTemporaryFileInDir(temp_directory_.GetPath(),
                                         &response_path));

    CHECK(base::WriteFile(response_path, test_response->data));

    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(
            &BackgroundFetchDelegate::Client::OnDownloadComplete,
            job_id_to_client_map_[job_unique_id], job_unique_id, guid,
            std::make_unique<BackgroundFetchResult>(
                std::make_unique<BackgroundFetchResponse>(
                    std::vector<GURL>({url}), test_response->headers),
                base::Time::Now(), response_path,
                /* blob_handle= */ std::nullopt, test_response->data.size())));
  } else {
    auto response = std::make_unique<BackgroundFetchResponse>(
        std::vector<GURL>({url}), test_response->headers);
    auto result = std::make_unique<BackgroundFetchResult>(
        std::move(response), base::Time::Now(),
        BackgroundFetchResult::FailureReason::FETCH_ERROR);
    PostAbortCheckingTask(
        job_unique_id,
        base::BindOnce(&BackgroundFetchDelegate::Client::OnDownloadComplete,
                       job_id_to_client_map_[job_unique_id], job_unique_id,
                       guid, std::move(result)));
  }

  seen_guids_.insert(guid);
}

void MockBackgroundFetchDelegate::Abort(const std::string& job_unique_id) {
  aborted_jobs_.insert(job_unique_id);
}

void MockBackgroundFetchDelegate::MarkJobComplete(
    const std::string& job_unique_id) {
  completed_jobs_.insert(job_unique_id);
}

void MockBackgroundFetchDelegate::UpdateUI(
    const std::string& job_unique_id,
    const std::optional<std::string>& title,
    const std::optional<SkBitmap>& icon) {
  job_id_to_client_map_[job_unique_id]->OnUIUpdated(job_unique_id);
}

void MockBackgroundFetchDelegate::RegisterResponse(
    const GURL& url,
    std::unique_ptr<TestResponse> response) {
  DCHECK_EQ(0u, url_responses_.count(url));
  url_responses_[url] = std::move(response);
}

void MockBackgroundFetchDelegate::PostAbortCheckingTask(
    const std::string& job_unique_id,
    base::OnceCallback<void()> callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockBackgroundFetchDelegate::RunAbortCheckingTask,
                     base::Unretained(this), job_unique_id,
                     std::move(callback)));
}

void MockBackgroundFetchDelegate::RunAbortCheckingTask(
    const std::string& job_unique_id,
    base::OnceCallback<void()> callback) {
  if (!aborted_jobs_.count(job_unique_id))
    std::move(callback).Run();
}

}  // namespace content
