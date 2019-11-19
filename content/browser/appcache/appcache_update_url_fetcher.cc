// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_url_fetcher.h"

#include <memory>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/appcache/appcache_update_url_loader_request.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"

namespace content {

namespace {

const int kMaxRetryCount = 3;

}  // namespace

AppCacheUpdateJob::URLFetcher::URLFetcher(const GURL& url,
                                          FetchType fetch_type,
                                          AppCacheUpdateJob* job,
                                          int buffer_size)
    : url_(url),
      job_(job),
      fetch_type_(fetch_type),
      buffer_size_(buffer_size),
      request_(std::make_unique<UpdateURLLoaderRequest>(
          job->service_->partition(),
          url,
          buffer_size,
          this)) {
  DCHECK(job != nullptr);
}

AppCacheUpdateJob::URLFetcher::~URLFetcher() = default;

void AppCacheUpdateJob::URLFetcher::Start() {
  request_->SetSiteForCookies(job_->manifest_url_);
  request_->SetInitiator(url::Origin::Create(job_->manifest_url_));
  if (fetch_type_ == FetchType::kManifest && job_->doing_full_update_check_) {
    request_->SetLoadFlags(request_->GetLoadFlags() | net::LOAD_BYPASS_CACHE);
  } else if (existing_response_headers_.get()) {
    AddConditionalHeaders(existing_response_headers_.get());
  }
  request_->Start();
}

void AppCacheUpdateJob::URLFetcher::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info) {
  DCHECK(request_);
  // Redirect is not allowed by the update process.
  job_->MadeProgress();
  redirect_response_code_ = request_->GetResponseCode();
  request_->Cancel();
  result_ = AppCacheUpdateJob::REDIRECT_ERROR;
  OnResponseCompleted(net::ERR_ABORTED);
}

void AppCacheUpdateJob::URLFetcher::OnResponseStarted(int net_error) {
  DCHECK(request_);
  DCHECK_NE(net::ERR_IO_PENDING, net_error);

  int response_code = -1;
  if (net_error == net::OK) {
    response_code = request_->GetResponseCode();
    job_->MadeProgress();
  }

  if ((response_code / 100) != 2) {
    if (response_code > 0)
      result_ = AppCacheUpdateJob::SERVER_ERROR;
    else
      result_ = AppCacheUpdateJob::NETWORK_ERROR;
    OnResponseCompleted(net_error);
    return;
  }

  if (url_.SchemeIsCryptographic()) {
    // Do not cache content with cert errors.
    // Also, we willfully violate the HTML5 spec at this point in order
    // to support the appcaching of cross-origin HTTPS resources.
    // We've opted for a milder constraint and allow caching unless
    // the resource has a "no-store" header. A spec change has been
    // requested on the whatwg list.
    // See http://code.google.com/p/chromium/issues/detail?id=69594
    // TODO(michaeln): Consider doing this for cross-origin HTTP too.
    if ((net::IsCertStatusError(
             request_->GetResponseInfo().ssl_info.cert_status) &&
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kIgnoreCertificateErrors)) ||
        (url_.GetOrigin() != job_->manifest_url_.GetOrigin() &&
         request_->GetResponseHeaders()->HasHeaderValue("cache-control",
                                                        "no-store"))) {
      DCHECK_EQ(-1, redirect_response_code_);
      request_->Cancel();
      result_ = AppCacheUpdateJob::SECURITY_ERROR;
      OnResponseCompleted(net::ERR_ABORTED);
      return;
    }
  }

  if (fetch_type_ == FetchType::kManifest ||
      fetch_type_ == FetchType::kManifestRefetch) {
    ReadResponseData();
    return;
  }

  // Write response info to storage for non-manifest fetches. Wait for async
  // write completion before reading any response data.
  DCHECK(fetch_type_ == FetchType::kResource ||
         fetch_type_ == FetchType::kNewMasterEntry);
  response_writer_ = job_->CreateResponseWriter();
  scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
      base::MakeRefCounted<HttpResponseInfoIOBuffer>(
          std::make_unique<net::HttpResponseInfo>(request_->GetResponseInfo()));
  response_writer_->WriteInfo(
      io_buffer.get(),
      base::BindOnce(&URLFetcher::OnWriteComplete, base::Unretained(this)));
}

void AppCacheUpdateJob::URLFetcher::OnReadCompleted(net::IOBuffer* buffer,
                                                    int bytes_read) {
  DCHECK(request_);
  DCHECK_NE(net::ERR_IO_PENDING, bytes_read);

  if (bytes_read <= 0) {
    OnResponseCompleted(bytes_read);
    return;
  }

  job_->MadeProgress();
  if (ConsumeResponseData(buffer, bytes_read))
    request_->Read();
}

void AppCacheUpdateJob::URLFetcher::AddConditionalHeaders(
    const net::HttpResponseHeaders* headers) {
  DCHECK(request_);
  DCHECK(headers);
  net::HttpRequestHeaders extra_headers;

  // Add If-Modified-Since header if response info has Last-Modified header.
  const std::string last_modified = "Last-Modified";
  std::string last_modified_value;
  headers->EnumerateHeader(nullptr, last_modified, &last_modified_value);
  if (!last_modified_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                            last_modified_value);
  }

  // Add If-None-Match header if response info has ETag header.
  const std::string etag = "ETag";
  std::string etag_value;
  headers->EnumerateHeader(nullptr, etag, &etag_value);
  if (!etag_value.empty()) {
    extra_headers.SetHeader(net::HttpRequestHeaders::kIfNoneMatch, etag_value);
  }
  if (!extra_headers.IsEmpty())
    request_->SetExtraRequestHeaders(extra_headers);
}

void AppCacheUpdateJob::URLFetcher::OnWriteComplete(int result) {
  if (result < 0) {
    request_->Cancel();
    result_ = AppCacheUpdateJob::DISKCACHE_ERROR;
    OnResponseCompleted(net::ERR_ABORTED);
    return;
  }
  ReadResponseData();
}

void AppCacheUpdateJob::URLFetcher::ReadResponseData() {
  AppCacheUpdateJob::InternalUpdateState state = job_->internal_state_;
  if (state == AppCacheUpdateJob::CACHE_FAILURE ||
      state == AppCacheUpdateJob::CANCELLED ||
      state == AppCacheUpdateJob::COMPLETED) {
    return;
  }
  request_->Read();
}

// Returns false if response data is processed asynchronously, in which
// case ReadResponseData will be invoked when it is safe to continue
// reading more response data from the request.
bool AppCacheUpdateJob::URLFetcher::ConsumeResponseData(net::IOBuffer* buffer,
                                                        int bytes_read) {
  DCHECK_GT(bytes_read, 0);

  if (fetch_type_ == FetchType::kManifest ||
      fetch_type_ == FetchType::kManifestRefetch) {
    manifest_data_.append(buffer->data(), bytes_read);
    return true;
  }

  DCHECK(fetch_type_ == FetchType::kResource ||
         fetch_type_ == FetchType::kNewMasterEntry);

  DCHECK(response_writer_.get());
  response_writer_->WriteData(
      buffer, bytes_read,
      base::BindOnce(&URLFetcher::OnWriteComplete, base::Unretained(this)));
  return false;  // wait for async write completion to continue reading
}

void AppCacheUpdateJob::URLFetcher::OnResponseCompleted(int net_error) {
  if (net_error == net::OK) {
    job_->MadeProgress();
  } else if (result_ == AppCacheUpdateJob::UPDATE_OK) {
    result_ = AppCacheUpdateJob::NETWORK_ERROR;
  }

  // Retry for 503s where retry-after is 0.
  if (net_error == net::OK && request_->GetResponseCode() == 503 &&
      MaybeRetryRequest()) {
    return;
  }

  switch (fetch_type_) {
    case FetchType::kResource:
      job_->HandleResourceFetchCompleted(this, net_error);
      break;
    case FetchType::kNewMasterEntry:
      job_->HandleNewMasterEntryFetchCompleted(this, net_error);
      break;
    case FetchType::kManifest:
      job_->HandleManifestFetchCompleted(this, net_error);
      break;
    case FetchType::kManifestRefetch:
      job_->HandleManifestRefetchCompleted(this, net_error);
      break;
    default:
      NOTREACHED();
  }
}

bool AppCacheUpdateJob::URLFetcher::MaybeRetryRequest() {
  if (retry_count_ >= kMaxRetryCount ||
      !request_->GetResponseHeaders()->HasHeaderValue("retry-after", "0")) {
    return false;
  }
  ++retry_count_;

  result_ = AppCacheUpdateJob::UPDATE_OK;
  request_ = std::make_unique<UpdateURLLoaderRequest>(
      job_->service_->partition(), url_, buffer_size_, this);
  Start();
  return true;
}

}  // namespace content.
