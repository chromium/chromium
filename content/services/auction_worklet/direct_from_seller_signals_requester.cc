// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/direct_from_seller_signals_requester.h"

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-local-handle.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-value.h"

namespace auction_worklet {

namespace {

// Validates that the Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only)
// header is present. Returns std::nullopt upon success. Upon failure, returns
// an error string.
//
// NOTE: This check is *NOT* directly part of the DirectFromSellerSignals
// security model, and serves more as a convenience check for developers: the
// network service and browser process ensure that resources that have the
// "Ad-Auction-Only: true" header are only usable in FLEDGE auctions. This
// check reminds developers using DirectFromSellerSignals to use
// Ad-Auction-Only on subresource responses to ensure that these responses
// are protected (by the browser and network stack) from being using outside
// FLEDGE.
std::optional<std::string> CheckHeader(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  // TODO(crbug.com/40269364): Remove support for old header names once API
  // users have switched.
  std::string old_header_value;
  std::string new_header_value;
  // TODO(crbug.com/40269364): Remove old names once API users have migrated to
  // new names.
  const bool got_new_header =
      headers->GetNormalizedHeader("Ad-Auction-Only", &new_header_value);
  const bool got_old_header =
      headers->GetNormalizedHeader("X-FLEDGE-Auction-Only", &old_header_value);
  if (!got_new_header && !got_old_header) {
    return "Missing Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only) "
           "header.";
  }
  if (got_old_header) {
    if (got_new_header) {
      if (old_header_value != new_header_value) {
        return base::StringPrintf(
            "Ad-Auction-Only: %s does not match deprecated header "
            "X-FLEDGE-Auction-Only: %s.",
            new_header_value.c_str(), old_header_value.c_str());
      }
    } else {
      new_header_value = std::move(old_header_value);
    }
  }
  if (!base::EqualsCaseInsensitiveASCII(new_header_value, "true")) {
    return base::StringPrintf(
        "Wrong Ad-Auction-Only (or deprecated X-FLEDGE-Auction-Only) header "
        "value. Expected \"true\", found "
        "\"%s\".",
        new_header_value.c_str());
  }

  return std::nullopt;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DirectFromSellerSignalsRequestType {
  kNetworkServiceFetch = 0,
  kCache = 1,
  kCoalesced = 2,
  kMaxValue = kCoalesced,
};

}  // namespace

DirectFromSellerSignalsRequester::Result::Result() = default;

DirectFromSellerSignalsRequester::Result::Result(Result&&) = default;

DirectFromSellerSignalsRequester::Result&
DirectFromSellerSignalsRequester::Result::operator=(Result&&) = default;

DirectFromSellerSignalsRequester::Result::~Result() = default;

v8::Local<v8::Value> DirectFromSellerSignalsRequester::Result::GetSignals(
    AuctionV8Helper& v8_helper,
    v8::Local<v8::Context> context,
    std::vector<std::string>& errors) const {
  DCHECK(v8_helper.v8_runner()->RunsTasksInCurrentSequence());
  if (absl::holds_alternative<ErrorString>(response_or_error_)) {
    errors.push_back(absl::get<ErrorString>(response_or_error_).value());
    return v8::Null(v8_helper.isolate());
  }

  DCHECK(absl::holds_alternative<scoped_refptr<ResponseString>>(
      response_or_error_));
  scoped_refptr<ResponseString> response =
      absl::get<scoped_refptr<ResponseString>>(response_or_error_);
  v8::MaybeLocal<v8::Value> v8_result =
      response ? v8_helper.CreateValueFromJson(context, response->value())
               : v8::Null(v8_helper.isolate());
  if (v8_result.IsEmpty()) {
    errors.push_back(base::StringPrintf(
        "DirectFromSellerSignals response for URL %s is not valid JSON.",
        signals_url_.spec().c_str()));
    return v8::Null(v8_helper.isolate());
  }
  return v8_result.ToLocalChecked();
}

bool DirectFromSellerSignalsRequester::Result::IsNull() const {
  if (absl::holds_alternative<ErrorString>(response_or_error_)) {
    return false;
  }

  DCHECK(absl::holds_alternative<scoped_refptr<ResponseString>>(
      response_or_error_));
  scoped_refptr<ResponseString> response =
      absl::get<scoped_refptr<ResponseString>>(response_or_error_);
  return response == nullptr;
}

DirectFromSellerSignalsRequester::Result::ResponseString::ResponseString(
    std::string&& other)
    : value_(std::move(other)) {}

DirectFromSellerSignalsRequester::Result::ResponseString::~ResponseString() =
    default;

DirectFromSellerSignalsRequester::Result::Result(
    GURL signals_url,
    std::unique_ptr<std::string> response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error)
    : signals_url_(std::move(signals_url)) {
  DCHECK(!signals_url_.is_empty());
  if (response_body) {
    DCHECK(!error);
    error = CheckHeader(headers);
  }
  if (error) {
    response_or_error_ = ErrorString(std::move(*error));
  } else {
    response_or_error_ =
        base::MakeRefCounted<ResponseString>(std::move(*response_body));
  }
}

DirectFromSellerSignalsRequester::Result::Result(const Result&) = default;

DirectFromSellerSignalsRequester::Result&
DirectFromSellerSignalsRequester::Result::operator=(const Result&) = default;

DirectFromSellerSignalsRequester::Request::~Request() {
  requester_->OnRequestDestroyed(*this);
}

DirectFromSellerSignalsRequester::Request::Request(
    DirectFromSellerSignalsRequesterCallback callback,
    DirectFromSellerSignalsRequester& requester,
    const GURL& signals_url)
    : callback_(std::move(callback)),
      requester_(&requester),
      signals_url_(signals_url) {
  DCHECK(!callback_.is_null());
  DCHECK(!signals_url.is_empty());
}

void DirectFromSellerSignalsRequester::Request::RunCallbackSync(Result result) {
  // Running the callback indicates completion -- therefore, there is no need to
  // try to cancel the request upon completion since OnSignalsDownloaded()
  // should handle cleanup.
  maybe_coalesce_iterator_.reset();

  std::move(callback_).Run(std::move(result));
  // The callback may have destroyed `this`.
}

void DirectFromSellerSignalsRequester::Request::RunCallbackAsync(
    Result result) {
  DCHECK(!maybe_coalesce_iterator_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&Request::RunCallbackSync,
                                weak_factory_.GetWeakPtr(), std::move(result)));
}

DirectFromSellerSignalsRequester::DirectFromSellerSignalsRequester() = default;

DirectFromSellerSignalsRequester::~DirectFromSellerSignalsRequester() {
  // All Request objects should have been destroyed before this
  // DirectFromSellerSignalsRequester, emptying `coalesced_downloads_`.
  DCHECK_EQ(coalesced_downloads_.size(), 0u);
}

std::unique_ptr<DirectFromSellerSignalsRequester::Request>
DirectFromSellerSignalsRequester::LoadSignals(
    network::mojom::URLLoaderFactory& url_loader_factory,
    const GURL& signals_url,
    DirectFromSellerSignalsRequesterCallback callback) {
  auto request =
      base::WrapUnique(new Request(std::move(callback), *this, signals_url));

  if (cached_result_.signals_url() == signals_url) {
    // Request completed from cache -- done.
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Auction.DirectFromSellerSignals.RequestType",
        DirectFromSellerSignalsRequestType::kCache);
    request->RunCallbackAsync(cached_result_);
    return request;
  }

  auto it = coalesced_downloads_.find(signals_url);
  if (it == coalesced_downloads_.end()) {
    // No download currently is running for `signals_url` -- start one.
    bool inserted;
    std::tie(it, inserted) = coalesced_downloads_.emplace(
        signals_url,
        CoalescedDownload(std::make_unique<AuctionDownloader>(
            &url_loader_factory, signals_url,
            AuctionDownloader::DownloadMode::kActualDownload,
            AuctionDownloader::MimeType::kJson,
            /*post_body=*/std::nullopt,
            /*content_type=*/std::nullopt,
            AuctionDownloader::ResponseStartedCallback(),
            base::BindOnce(
                &DirectFromSellerSignalsRequester::OnSignalsDownloaded,
                base::Unretained(this), signals_url, base::TimeTicks::Now()),
            /*network_events_delegate=*/nullptr)));
    DCHECK(inserted);
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Auction.DirectFromSellerSignals.RequestType",
        DirectFromSellerSignalsRequestType::kNetworkServiceFetch);
  } else {
    base::UmaHistogramEnumeration(
        "Ads.InterestGroup.Auction.DirectFromSellerSignals.RequestType",
        DirectFromSellerSignalsRequestType::kCoalesced);
  }
  // A download is running for `signals_url` -- register to receive the result,
  // and register the iterator with `request` so that the Request destructor can
  // cancel. If the request is destroyed before the request completes, its
  // destructor will remove the request pointer from the `requests` list.
  request->set_coalesce_iterator(
      it->second.requests.emplace(it->second.requests.end(), request.get()));

  return request;
}

DirectFromSellerSignalsRequester::CoalescedDownload::CoalescedDownload(
    std::unique_ptr<AuctionDownloader> downloader)
    : downloader(std::move(downloader)) {}

DirectFromSellerSignalsRequester::CoalescedDownload::~CoalescedDownload() {
  DCHECK_EQ(0u, requests.size());
}

DirectFromSellerSignalsRequester::CoalescedDownload::CoalescedDownload(
    CoalescedDownload&&) = default;

DirectFromSellerSignalsRequester::CoalescedDownload&
DirectFromSellerSignalsRequester::CoalescedDownload::operator=(
    CoalescedDownload&&) = default;

void DirectFromSellerSignalsRequester::OnSignalsDownloaded(
    GURL signals_url,
    base::TimeTicks start_time,
    std::unique_ptr<std::string> response_body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error) {
  if (response_body) {
    // The request size isn't very meaningful, since the request is served from
    // a subresource bundle, so don't record the request size.
    base::UmaHistogramCounts10M(
        "Ads.InterestGroup.Net.ResponseSizeBytes.DirectFromSellerSignals",
        response_body->size());
    base::UmaHistogramTimes(
        "Ads.InterestGroup.Net.DownloadTime.DirectFromSellerSignals",
        base::TimeTicks::Now() - start_time);
  }
  Result result(signals_url, std::move(response_body), std::move(headers),
                std::move(error));
  cached_result_ = result;

  // Move the request pointers and clear the `coalesced_downloads_` pair -- this
  // will also destroy the downloader. The Request won't try to cancel anything
  // after this since running the callback clears the Request's iterator.
  auto it = coalesced_downloads_.find(signals_url);
  CHECK(it != coalesced_downloads_.end(), base::NotFatalUntil::M130);
  DCHECK_EQ(signals_url, it->second.downloader->source_url());
  std::list<raw_ptr<Request>> requests;
  std::swap(requests, it->second.requests);
  coalesced_downloads_.erase(it);

  while (!requests.empty()) {
    // `*request` may be destroyed by the callback, so we also don't want to
    // keep a dangling pointer to it in `requests`.
    Request* request = requests.front();
    requests.pop_front();
    request->RunCallbackSync(result);
  }
}

void DirectFromSellerSignalsRequester::OnRequestDestroyed(Request& request) {
  DCHECK(request.requester_);
  // If signals were were retrieved from cache, or the request already
  // completed, no cleanup is necessary.
  if (!request.maybe_coalesce_iterator_) {
    return;
  }

  // Otherwise, remove the request pointer to `this` from
  // `coalesced_downloads_`.
  auto map_it = coalesced_downloads_.find(request.signals_url_);
  CHECK(map_it != coalesced_downloads_.end(), base::NotFatalUntil::M130);
  CoalescedDownload& coalesced_download = map_it->second;
  DCHECK_EQ(coalesced_download.downloader->source_url(), request.signals_url_);
  DCHECK_GT(coalesced_download.requests.size(), 0u);
  DCHECK_EQ(**request.maybe_coalesce_iterator_, &request);
  coalesced_download.requests.erase(*request.maybe_coalesce_iterator_);

  // If there are now no more requests left for `request.signals_url_`, delete
  // its `coalesced_downloads_` pair. This will cancel the download for that
  // URL.
  if (coalesced_download.requests.empty()) {
    coalesced_downloads_.erase(map_it);
  }
}

}  // namespace auction_worklet
