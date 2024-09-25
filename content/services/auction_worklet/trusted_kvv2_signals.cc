// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_kvv2_signals.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "content/services/auction_worklet/public/mojom/auction_network_events_handler.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/http/http_response_headers.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/buffers/oblivious_http_request.h"
#include "net/third_party/quiche/src/quiche/oblivious_http/oblivious_http_client.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/features_generated.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"

namespace auction_worklet {

std::unique_ptr<TrustedKVv2Signals> TrustedKVv2Signals::LoadKVv2BiddingSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        devtools_pending_remote,
    std::set<std::string> interest_group_names,
    std::set<std::string> bidding_signals_keys,
    const GURL& trusted_bidding_signals_url,
    std::unique_ptr<TrustedBiddingSignalsKVv2RequestHelperBuilder>
        request_helper_builder,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadKVv2SignalsCallback load_kvv2_signals_callback) {
  DCHECK(!interest_group_names.empty());

  std::unique_ptr<TrustedSignalsKVv2RequestHelper> request_helper =
      request_helper_builder->Build();

  std::unique_ptr<TrustedKVv2Signals> trusted_kvv2_signals =
      base::WrapUnique(new TrustedKVv2Signals(
          std::move(interest_group_names), std::move(bidding_signals_keys),
          /*render_urls=*/std::nullopt,
          /*ad_component_render_urls=*/std::nullopt,
          trusted_bidding_signals_url,
          request_helper->TakeOHttpRequestContext(),
          std::move(devtools_pending_remote), std::move(v8_helper),
          std::move(load_kvv2_signals_callback)));

  trusted_kvv2_signals->StartKVv2Download(
      url_loader_factory, trusted_bidding_signals_url,
      request_helper->TakePostRequestBody());

  return trusted_kvv2_signals;
}

std::unique_ptr<TrustedKVv2Signals> TrustedKVv2Signals::LoadKVv2ScoringSignals(
    network::mojom::URLLoaderFactory* url_loader_factory,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        devtools_pending_remote,
    std::set<std::string> render_urls,
    std::set<std::string> ad_component_render_urls,
    const GURL& trusted_scoring_signals_url,
    std::unique_ptr<TrustedScoringSignalsKVv2RequestHelperBuilder>
        request_helper_builder,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadKVv2SignalsCallback load_kvv2_signals_callback) {
  DCHECK(!render_urls.empty());

  std::unique_ptr<TrustedSignalsKVv2RequestHelper> request_helper =
      request_helper_builder->Build();

  std::unique_ptr<TrustedKVv2Signals> trusted_kvv2_signals =
      base::WrapUnique(new TrustedKVv2Signals(
          /*interest_group_names=*/std::nullopt,
          /*bidding_signals_keys=*/std::nullopt, std::move(render_urls),
          std::move(ad_component_render_urls), trusted_scoring_signals_url,
          request_helper->TakeOHttpRequestContext(),
          std::move(devtools_pending_remote), std::move(v8_helper),
          std::move(load_kvv2_signals_callback)));

  trusted_kvv2_signals->StartKVv2Download(
      url_loader_factory, trusted_scoring_signals_url,
      request_helper->TakePostRequestBody());

  return trusted_kvv2_signals;
}

TrustedKVv2Signals::TrustedKVv2Signals(
    std::optional<std::set<std::string>> interest_group_names,
    std::optional<std::set<std::string>> bidding_signals_keys,
    std::optional<std::set<std::string>> render_urls,
    std::optional<std::set<std::string>> ad_component_render_urls,
    const GURL& trusted_signals_url,
    quiche::ObliviousHttpRequest::Context context,
    mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
        auction_network_events_handler,
    scoped_refptr<AuctionV8Helper> v8_helper,
    LoadKVv2SignalsCallback load_kvv2_signals_callback)
    : interest_group_names_(std::move(interest_group_names)),
      bidding_signals_keys_(std::move(bidding_signals_keys)),
      render_urls_(std::move(render_urls)),
      ad_component_render_urls_(std::move(ad_component_render_urls)),
      trusted_signals_url_(trusted_signals_url),
      v8_helper_(std::move(v8_helper)),
      load_kvv2_signals_callback_(std::move(load_kvv2_signals_callback)),
      context_(std::move(context)),
      auction_network_events_handler_(
          std::move(auction_network_events_handler)) {
  DCHECK(v8_helper_);
  DCHECK(load_kvv2_signals_callback_);

  // Either this should be for bidding signals or scoring signals.
  DCHECK((interest_group_names_ && bidding_signals_keys_) ||
         (render_urls_ && ad_component_render_urls_));
  DCHECK((!interest_group_names_ && !bidding_signals_keys_) ||
         (!render_urls_ && !ad_component_render_urls_));
}

TrustedKVv2Signals::~TrustedKVv2Signals() = default;

void TrustedKVv2Signals::StartKVv2Download(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const GURL& full_signals_url,
    std::string post_body) {
  download_start_time_ = base::TimeTicks::Now();

  std::unique_ptr<MojoNetworkEventsDelegate> network_events_delegate;

  if (auction_network_events_handler_.is_valid()) {
    network_events_delegate = std::make_unique<MojoNetworkEventsDelegate>(
        std::move(auction_network_events_handler_));
  }
  auction_downloader_ = std::make_unique<AuctionDownloader>(
      url_loader_factory, full_signals_url,
      AuctionDownloader::DownloadMode::kActualDownload,
      AuctionDownloader::MimeType::kAdAuctionTrustedSignals,
      std::move(post_body),
      /*content_type=*/kTrustedSignalsKVv2EncryptionRequestMediaType,
      AuctionDownloader::ResponseStartedCallback(),
      base::BindOnce(&TrustedKVv2Signals::OnKVv2DownloadComplete,
                     base::Unretained(this)),
      /*network_events_delegate=*/std::move(network_events_delegate));
}

void TrustedKVv2Signals::OnKVv2DownloadComplete(
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    std::optional<std::string> error_msg) {
  // The downloader's job is done, so clean it up.
  auction_downloader_.reset();

  // Key-related fields aren't needed after this call, so pass ownership of them
  // over to the parser on the V8 thread.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &TrustedKVv2Signals::HandleKVv2DownloadResultOnV8Thread, v8_helper_,
          trusted_signals_url_, std::move(interest_group_names_),
          std::move(bidding_signals_keys_), std::move(render_urls_),
          std::move(ad_component_render_urls_), std::move(body),
          std::move(headers), std::move(context_), std::move(error_msg),
          base::SequencedTaskRunner::GetCurrentDefault(),
          weak_ptr_factory.GetWeakPtr(),
          base::TimeTicks::Now() - download_start_time_));
}

// static
void TrustedKVv2Signals::HandleKVv2DownloadResultOnV8Thread(
    scoped_refptr<AuctionV8Helper> v8_helper,
    const GURL& signals_url,
    std::optional<std::set<std::string>> interest_group_names,
    std::optional<std::set<std::string>> bidding_signals_keys,
    std::optional<std::set<std::string>> render_urls,
    std::optional<std::set<std::string>> ad_component_render_urls,
    std::unique_ptr<std::string> body,
    scoped_refptr<net::HttpResponseHeaders> headers,
    quiche::ObliviousHttpRequest::Context context,
    std::optional<std::string> error_msg,
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedKVv2Signals> weak_instance,
    base::TimeDelta download_time) {
  if (!body) {
    PostKVv2CallbackToUserThread(std::move(user_thread_task_runner),
                                 weak_instance, /*result_map=*/std::nullopt,
                                 std::move(error_msg));
    return;
  }
  DCHECK(!error_msg.has_value());

  auto maybe_fetch_result =
      TrustedSignalsKVv2ResponseParser::ParseResponseToSignalsFetchResult(
          *body, context);
  if (!maybe_fetch_result.has_value()) {
    PostKVv2CallbackToUserThread(
        std::move(user_thread_task_runner), weak_instance,
        /*result_map=*/std::nullopt,
        std::move(maybe_fetch_result).error().error_msg);
    return;
  }

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper.get());
  v8::Context::Scope context_scope(v8_helper->scratch_context());

  if (bidding_signals_keys) {
    CHECK(interest_group_names.has_value());
    TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError
        maybe_result_map = TrustedSignalsKVv2ResponseParser::
            ParseBiddingSignalsFetchResultToResultMap(
                v8_helper.get(), interest_group_names.value(),
                bidding_signals_keys.value(), maybe_fetch_result.value());

    if (!maybe_result_map.has_value()) {
      PostKVv2CallbackToUserThread(
          std::move(user_thread_task_runner), weak_instance,
          /*result_map=*/std::nullopt,
          std::move(maybe_result_map).error().error_msg);
      return;
    }

    PostKVv2CallbackToUserThread(
        std::move(user_thread_task_runner), weak_instance,
        std::move(maybe_result_map).value(), std::move(error_msg));
  } else {
    // Handle scoring signals case.
    CHECK(render_urls.has_value());
    TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMapOrError
        maybe_result_map = TrustedSignalsKVv2ResponseParser::
            ParseScoringSignalsFetchResultToResultMap(
                v8_helper.get(), render_urls.value(),
                ad_component_render_urls.value(), maybe_fetch_result.value());

    if (!maybe_result_map.has_value()) {
      PostKVv2CallbackToUserThread(
          std::move(user_thread_task_runner), weak_instance,
          /*result_map=*/std::nullopt,
          std::move(maybe_result_map).error().error_msg);
      return;
    }

    PostKVv2CallbackToUserThread(
        std::move(user_thread_task_runner), weak_instance,
        std::move(maybe_result_map).value(), std::move(error_msg));
  }
}

void TrustedKVv2Signals::PostKVv2CallbackToUserThread(
    scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
    base::WeakPtr<TrustedKVv2Signals> weak_instance,
    std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
        result_map,
    std::optional<std::string> error_msg) {
  user_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TrustedKVv2Signals::DeliverKVv2CallbackOnUserThread,
                     weak_instance, std::move(result_map),
                     std::move(error_msg)));
}

void TrustedKVv2Signals::DeliverKVv2CallbackOnUserThread(
    std::optional<TrustedSignalsKVv2ResponseParser::TrustedSignalsResultMap>
        result_map,
    std::optional<std::string> error_msg) {
  std::move(load_kvv2_signals_callback_)
      .Run(std::move(result_map), std::move(error_msg));
}

}  // namespace auction_worklet
