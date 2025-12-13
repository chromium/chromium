// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ACCEPT_CH_FRAME_INTERCEPTOR_H_
#define SERVICES_NETWORK_ACCEPT_CH_FRAME_INTERCEPTOR_H_

#include <memory>

#include "base/component_export.h"
#include "base/types/pass_key.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/completion_once_callback.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/accept_ch_frame_observer.mojom.h"

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace network {

// Handles the logic associated with processing the HTTP/2 or HTTP/3
// `ACCEPT_CH` frame received during connection establishment.
//
// This class parses the client hints advertised in the frame, compares them
// against hints already included in the initial request headers, and interacts
// with a browser-provided `AcceptCHFrameObserver` to potentially restart the
// request if additional client hints should be included.
class COMPONENT_EXPORT(NETWORK_SERVICE) AcceptCHFrameInterceptor {
 public:
  // Conditionally creates an instance of AcceptCHFrameInterceptor.
  // Returns nullptr if the `accept_ch_frame_observer` is invalid or if the
  // kAcceptCHFrame feature flag is disabled. Otherwise, returns a unique_ptr
  // to a new instance.
  // `enabled_client_hints` represents the client hints enabled when the
  // resource request was created.
  static std::unique_ptr<AcceptCHFrameInterceptor> MaybeCreate(
      mojo::PendingRemote<mojom::AcceptCHFrameObserver>
          accept_ch_frame_observer,
      std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
          enabled_client_hints);

  static std::unique_ptr<AcceptCHFrameInterceptor> CreateForTesting(
      mojo::PendingRemote<mojom::AcceptCHFrameObserver>
          accept_ch_frame_observer,
      std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
          enabled_client_hints);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(NeedsObserverCheckReason)
  enum class NeedsObserverCheckReason {
    kNotNeeded = 0,  // The case where it returns false.
    kNoEnabledClientHints = 1,
    kMainFrameOriginMismatch = 2,
    kSubframeFeatureDisabled = 3,
    kHintNotEnabled = 4,
    kMaxValue = kHintNotEnabled,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:NeedsObserverCheckReason)

  AcceptCHFrameInterceptor(const AcceptCHFrameInterceptor&) = delete;
  AcceptCHFrameInterceptor& operator=(const AcceptCHFrameInterceptor&) = delete;

  ~AcceptCHFrameInterceptor();

  // Called when connection establishment details are available, including any
  // information potentially derived from an HTTP/2 or HTTP/3 `ACCEPT_CH` frame.
  // It takes the request's `url`, a string representation (`accept_ch_frame`)
  // of the hints advertised in such a frame, and the initial outgoing request
  // `headers`. It compares these advertised hints against the `headers`. If any
  // hints needed for the request (based on the frame) were not already
  // included, this method notifies the `accept_ch_frame_observer_`.
  //
  // Returns `net::OK` if no observer notification is needed. If `net::OK` is
  // returned, the request can proceed immediately, `callback` will not be
  // called.
  // Returns `net::ERR_IO_PENDING` if the observer is notified. If
  // `net::ERR_IO_PENDING` is returned, the `callback` will be invoked
  // asynchronously by this class once the observer responds via Mojo
  // (the response usually indicates `net::OK` to proceed or an error).
  net::Error OnConnected(const GURL& url,
                         const std::string& accept_ch_frame,
                         const net::HttpRequestHeaders& headers,
                         net::CompletionOnceCallback callback);

  NeedsObserverCheckReason NeedsObserverCheckForTesting(
      const url::Origin& origin,
      const std::vector<mojom::WebClientHintsType>& hints);

 private:
  // Constructor is private to enforce creation via MaybeCreate or
  // CreateForTesting.
  AcceptCHFrameInterceptor(
      mojo::PendingRemote<mojom::AcceptCHFrameObserver>
          accept_ch_frame_observer,
      std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
          enabled_client_hints,
      base::PassKey<AcceptCHFrameInterceptor>);

  // Returns true if `hints` need to be checked by the
  // `accept_ch_frame_observer_`.
  // If hints requested by the ACCEPT_CH frame is not in the resource request,
  // the IPC is sent to the browser process via `accept_ch_frame_observer_` to
  // verify if it is enabled.
  // `hints` represents the difference between ACCEPT_CH frame and the resource
  // request. This function verifies if `hints` are enabled at the request
  // creation time (which is stored in `enabled_client_hints_`) to avoid sending
  // the IPCs if possible. The enabled hints may change after the request
  // creation, and the IPC is still needed for them.
  NeedsObserverCheckReason NeedsObserverCheck(
      const url::Origin& origin,
      const std::vector<mojom::WebClientHintsType>& hints);

  mojo::Remote<mojom::AcceptCHFrameObserver> accept_ch_frame_observer_;

  // `enabled_client_hints_` is the client hints enabled when the
  // resource request was created. If ACCEPT_CH frame has client hints
  // which does not show up in a request header during `OnConnected()`,
  // `accept_ch_observer_` should be called to ensure the hints are enabled.
  // `enabled_client_hints_` is used beforehand to avoid an unnecessary observer
  // call.
  const std::optional<ResourceRequest::TrustedParams::EnabledClientHints>
      enabled_client_hints_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_ACCEPT_CH_FRAME_INTERCEPTOR_H_
