// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/accept_ch_frame_interceptor.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"

namespace network {

namespace {

// Parses AcceptCHFrame and removes client hints already in the headers.
std::vector<mojom::WebClientHintsType> ComputeAcceptCHFrameHints(
    const std::string& accept_ch_frame,
    const net::HttpRequestHeaders& headers) {
  std::optional<std::vector<mojom::WebClientHintsType>> maybe_hints =
      ParseClientHintsHeader(accept_ch_frame);

  if (!maybe_hints) {
    return {};
  }

  // Only look at/add headers that aren't already present.
  std::vector<mojom::WebClientHintsType> hints;
  for (auto hint : maybe_hints.value()) {
    // ResourceWidth is only for images, which won't trigger a restart.
    if (hint == mojom::WebClientHintsType::kResourceWidth ||
        hint == mojom::WebClientHintsType::kResourceWidth_DEPRECATED) {
      continue;
    }

    const std::string header = GetClientHintToNameMap().at(hint);
    if (!headers.HasHeader(header)) {
      hints.push_back(hint);
    }
  }

  return hints;
}

}  // namespace

// static
std::unique_ptr<AcceptCHFrameInterceptor> AcceptCHFrameInterceptor::MaybeCreate(
    mojo::PendingRemote<mojom::AcceptCHFrameObserver>
        accept_ch_frame_observer) {
  if (!accept_ch_frame_observer ||
      !base::FeatureList::IsEnabled(features::kAcceptCHFrame)) {
    return nullptr;
  }
  return std::make_unique<AcceptCHFrameInterceptor>(
      std::move(accept_ch_frame_observer),
      base::PassKey<AcceptCHFrameInterceptor>());
}

AcceptCHFrameInterceptor::AcceptCHFrameInterceptor(
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    base::PassKey<AcceptCHFrameInterceptor>)
    : accept_ch_frame_observer_(std::move(accept_ch_frame_observer)) {}

AcceptCHFrameInterceptor::~AcceptCHFrameInterceptor() = default;

net::Error AcceptCHFrameInterceptor::OnConnected(
    const GURL& url,
    const std::string& accept_ch_frame,
    const net::HttpRequestHeaders& headers,
    net::CompletionOnceCallback callback) {
  if (accept_ch_frame.empty() || !accept_ch_frame_observer_) {
    return net::OK;
  }

  // Find client hints that are in the ACCEPT_CH frame that were not already
  // included in the request
  const auto hints = ComputeAcceptCHFrameHints(accept_ch_frame, headers);
  base::UmaHistogramBoolean("Net.URLLoader.AcceptCH.RunObserverCall",
                            !hints.empty());
  if (hints.empty()) {
    return net::OK;
  }

  // If there are hints in the ACCEPT_CH frame that weren't included in the
  // original request, notify the observer. If those hints can be included,
  // this URLLoader will be destroyed and another with the correct hints
  // started. Otherwise, the callback to continue the network transaction will
  // be called and the URLLoader will continue as normal.
  auto record = [](net::CompletionOnceCallback callback,
                   base::TimeTicks call_time, uint64_t trace_id, int status) {
    base::UmaHistogramMicrosecondsTimes("Net.URLLoader.AcceptCH.RoundTripTime",
                                        base::TimeTicks::Now() - call_time);
    base::UmaHistogramSparse("Net.URLLoader.AcceptCH.Status", -status);
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "loading", "AcceptCHObserver::OnAcceptCHFrameReceived call",
        TRACE_ID_LOCAL(trace_id), "status", status);
    std::move(callback).Run(status);
  };
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "loading", "AcceptCHObserver::OnAcceptCHFrameReceived call",
      TRACE_ID_LOCAL(this), "url", url);

  // Explanation of callback lifetime safety:
  // The `callback` originates from a net/ layer object (e.g.,
  // HttpNetworkTransaction) and might rely on an unretained pointer to that
  // object. The URLLoader which owns `this` manages the lifetime of `this`
  // (and its Mojo remote) and the net/ layer object associated with the
  // `callback`. So binding the `callback` here is safe.
  accept_ch_frame_observer_->OnAcceptCHFrameReceived(
      url::Origin::Create(url), hints,
      base::BindOnce(record, std::move(callback), base::TimeTicks::Now(),
                     TRACE_ID_LOCAL(this).raw_id()));
  return net::ERR_IO_PENDING;
}

}  // namespace network
