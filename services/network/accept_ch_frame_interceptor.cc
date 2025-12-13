// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/accept_ch_frame_interceptor.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
        enabled_client_hints) {
  if (!accept_ch_frame_observer ||
      !base::FeatureList::IsEnabled(features::kAcceptCHFrame)) {
    return nullptr;
  }
  return base::WrapUnique(new AcceptCHFrameInterceptor(
      std::move(accept_ch_frame_observer), std::move(enabled_client_hints),
      base::PassKey<AcceptCHFrameInterceptor>()));
}

std::unique_ptr<AcceptCHFrameInterceptor>
AcceptCHFrameInterceptor::CreateForTesting(
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
        enabled_client_hints) {
  return base::WrapUnique(new AcceptCHFrameInterceptor(
      std::move(accept_ch_frame_observer), std::move(enabled_client_hints),
      base::PassKey<AcceptCHFrameInterceptor>()));
}

AcceptCHFrameInterceptor::AcceptCHFrameInterceptor(
    mojo::PendingRemote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer,
    std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
        enabled_client_hints,
    base::PassKey<AcceptCHFrameInterceptor>)
    : accept_ch_frame_observer_(std::move(accept_ch_frame_observer)),
      enabled_client_hints_(std::move(enabled_client_hints)) {}

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

  const NeedsObserverCheckReason reason =
      NeedsObserverCheck(url::Origin::Create(url), hints);
  base::UmaHistogramEnumeration(
      "Net.AcceptCHFrameInterceptor.NeedsObserverCheckReason", reason);
  if (reason == NeedsObserverCheckReason::kNotNeeded) {
    return net::OK;
  }

  // If there are hints in the ACCEPT_CH frame that weren't included in the
  // original request, notify the observer. If those hints can be included,
  // this URLLoader will be destroyed and another with the correct hints
  // started. Otherwise, the callback to continue the network transaction will
  // be called and the URLLoader will continue as normal.
  auto record = [](net::CompletionOnceCallback callback,
                   base::TimeTicks call_time, perfetto::Track track,
                   int status) {
    base::UmaHistogramMicrosecondsTimes("Net.URLLoader.AcceptCH.RoundTripTime",
                                        base::TimeTicks::Now() - call_time);
    base::UmaHistogramSparse("Net.URLLoader.AcceptCH.Status", -status);
    TRACE_EVENT_END("loading", track, "status", status);
    std::move(callback).Run(status);
  };
  TRACE_EVENT_BEGIN("loading", "AcceptCHObserver::OnAcceptCHFrameReceived call",
                    perfetto::Track::FromPointer(this), "url", url);

  // Explanation of callback lifetime safety:
  // The `callback` originates from a net/ layer object (e.g.,
  // HttpNetworkTransaction) and might rely on an unretained pointer to that
  // object. The URLLoader which owns `this` manages the lifetime of `this`
  // (and its Mojo remote) and the net/ layer object associated with the
  // `callback`. So binding the `callback` here is safe.
  accept_ch_frame_observer_->OnAcceptCHFrameReceived(
      url::Origin::Create(url), hints,
      base::BindOnce(record, std::move(callback), base::TimeTicks::Now(),
                     perfetto::Track::FromPointer(this)));
  return net::ERR_IO_PENDING;
}

AcceptCHFrameInterceptor::NeedsObserverCheckReason
AcceptCHFrameInterceptor::NeedsObserverCheckForTesting(
    const url::Origin& origin,
    const std::vector<mojom::WebClientHintsType>& hints) {
  return NeedsObserverCheck(origin, hints);
}

AcceptCHFrameInterceptor::NeedsObserverCheckReason
AcceptCHFrameInterceptor::NeedsObserverCheck(
    const url::Origin& origin,
    const std::vector<mojom::WebClientHintsType>& hints) {
  if (!enabled_client_hints_.has_value()) {
    return NeedsObserverCheckReason::kNoEnabledClientHints;
  }

  // For main frames, the origin must match to use the cached hints.
  if (enabled_client_hints_->is_outermost_main_frame &&
      !enabled_client_hints_->origin.IsSameOriginWith(origin)) {
    return NeedsObserverCheckReason::kMainFrameOriginMismatch;
  }
  // For subframes, the optimization is only allowed if the feature is enabled.
  if (!enabled_client_hints_->is_outermost_main_frame &&
      !features::kAcceptCHOffloadForSubframe.Get()) {
    return NeedsObserverCheckReason::kSubframeFeatureDisabled;
  }

  CHECK(base::FeatureList::IsEnabled(features::kOffloadAcceptCHFrameCheck));
  // The Accept-CH frame can be offloaded (i.e., handled in the network
  // service without an IPC to the browser process) if all hints in the frame
  // are present in either the `hints` list (enabled and allowed) or the
  // `not_allowed_hints` list (persisted but currently disallowed). If any hint
  // is not in either list, we must fall back to the browser process to check.
  bool needs_observer_check = false;
  for (const auto& h : hints) {
    const bool is_in_hints = base::Contains(enabled_client_hints_->hints, h);
    const bool is_in_not_allowed_hints =
        features::kAcceptCHFrameOffloadNotAllowedHints.Get() &&
        base::Contains(enabled_client_hints_->not_allowed_hints, h);
    const bool is_valid_for_offload = is_in_hints || is_in_not_allowed_hints;
    if (is_in_not_allowed_hints && !is_in_hints) {
      base::UmaHistogramEnumeration(
          "Net.AcceptCHFrameInterceptor.OffloadSuccessForNotAllowedHint", h);
    }
    if (!is_valid_for_offload) {
      needs_observer_check = true;
      base::UmaHistogramEnumeration(
          "Net.AcceptCHFrameInterceptor.MismatchClientHint2", h);
    }
  }

  if (needs_observer_check) {
    return NeedsObserverCheckReason::kHintNotEnabled;
  }

  return NeedsObserverCheckReason::kNotNeeded;
}

}  // namespace network
