// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/protobuf_http_request_base.h"

#include <memory>

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/backoff_entry.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "remoting/base/protobuf_http_request_config.h"
#include "remoting/base/scoped_protobuf_http_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace remoting {

struct ProtobufHttpRequestBase::RetryEntry {
  explicit RetryEntry(
      const ProtobufHttpRequestConfig::RetryPolicy& retry_policy)
      : backoff_entry(retry_policy.backoff_policy) {
    retry_deadline = base::TimeTicks::Now() + retry_policy.retry_timeout;
  }

  ~RetryEntry() = default;

  net::BackoffEntry backoff_entry;
  base::OneShotTimer retry_timer;
  base::TimeTicks retry_deadline;
};

ProtobufHttpRequestBase::ProtobufHttpRequestBase(
    std::unique_ptr<ProtobufHttpRequestConfig> config)
    : config_(std::move(config)) {
  config_->Validate();
}

ProtobufHttpRequestBase::~ProtobufHttpRequestBase() {
#if DCHECK_IS_ON()
  DCHECK(request_deadline_.is_null() ||
         request_deadline_ >= base::TimeTicks::Now())
      << "The request must have been deleted before the deadline.";
#endif  // DCHECK_IS_ON()
}

std::unique_ptr<ScopedProtobufHttpRequest>
ProtobufHttpRequestBase::CreateScopedRequest() {
  return std::make_unique<ScopedProtobufHttpRequest>(base::BindOnce(
      &ProtobufHttpRequestBase::Invalidate, weak_factory_.GetWeakPtr()));
}

HttpStatus ProtobufHttpRequestBase::GetUrlLoaderStatus() const {
  net::Error net_error = static_cast<net::Error>(url_loader_->NetError());
  if (net_error != net::Error::OK &&
      net_error != net::Error::ERR_HTTP_RESPONSE_CODE_FAILURE) {
    return HttpStatus(net_error);
  }
  // Depending on the configuration, url_loader_->NetError() can be OK even if
  // the error code is 4xx or 5xx.
  if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers ||
      url_loader_->ResponseInfo()->headers->response_code() <= 0) {
    return HttpStatus(HttpStatus::Code::INTERNAL,
                      "Failed to get HTTP status from the response header.");
  }
  return HttpStatus(static_cast<net::HttpStatusCode>(
      url_loader_->ResponseInfo()->headers->response_code()));
}

bool ProtobufHttpRequestBase::HandleRetry(HttpStatus::Code code) {
  DCHECK(loader_factory_);

  // SimpleURLLoader supports retries, but it doesn't support retrying on
  // network error, and uses max retries rather than an absolute deadline for
  // setting the limit. Hence we use our own retry logic.

  static constexpr auto kRetriableErrorCodes =
      base::MakeFixedFlatSet<HttpStatus::Code>(
          {HttpStatus::Code::ABORTED, HttpStatus::Code::UNAVAILABLE,
           HttpStatus::Code::NETWORK_ERROR});

  if (!retry_entry_ || !kRetriableErrorCodes.contains(code)) {
    return false;
  }
  retry_entry_->backoff_entry.InformOfRequest(false);
  if (retry_entry_->backoff_entry.GetReleaseTime() >=
      retry_entry_->retry_deadline) {
    LOG(WARNING) << "No more retries remaining.";
    return false;
  }
  base::TimeDelta retry_delay =
      retry_entry_->backoff_entry.GetTimeUntilRelease();
  LOG(WARNING) << "Request failed with error code " << static_cast<int>(code)
               << ". It will be retried after " << retry_delay;
  retry_entry_->retry_timer.Start(FROM_HERE, retry_delay, this,
                                  &ProtobufHttpRequestBase::DoRequest);
  return true;
}

void ProtobufHttpRequestBase::StartRequest(
    network::mojom::URLLoaderFactory* loader_factory,
    CreateUrlLoader create_url_loader,
    base::OnceClosure invalidator) {
  DCHECK(!create_url_loader_);
  DCHECK(!invalidator_);

  if (config_->retry_policy) {
    retry_entry_ = std::make_unique<RetryEntry>(*config_->retry_policy);
  }
  loader_factory_ = loader_factory;
  create_url_loader_ = std::move(create_url_loader);
  invalidator_ = std::move(invalidator);
  DoRequest();
}

void ProtobufHttpRequestBase::DoRequest() {
  url_loader_ = create_url_loader_.Run(*config_);
  StartRequestInternal(loader_factory_);

#if DCHECK_IS_ON()
  base::TimeDelta timeout_duration = GetRequestTimeoutDuration();
  if (!timeout_duration.is_zero()) {
    // Add a 500ms fuzz to account for task dispatching delay and other stuff.
    request_deadline_ =
        base::TimeTicks::Now() + timeout_duration + base::Milliseconds(500);
  }
#endif  // DCHECK_IS_ON()
}

void ProtobufHttpRequestBase::Invalidate() {
  // This is not necessarily true if the request has never been added to a
  // client.
  if (invalidator_) {
    std::move(invalidator_).Run();
  }
}

}  // namespace remoting
