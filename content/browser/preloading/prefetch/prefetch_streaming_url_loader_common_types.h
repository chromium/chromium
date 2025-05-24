// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_COMMON_TYPES_H_
#define CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_COMMON_TYPES_H_

#include <optional>

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"

// Do NOT include `content/browser/preloading/` files,
// especially do NOT include `prefetch_streaming_url_loader.h`,
// to keep the dependencies clear.

// Various enums and type declarations used around `PrefetchStreamingURLLoader`
// and `PrefetchResponseReader`.

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceRequest;
struct URLLoaderCompletionStatus;
}  // namespace network

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class PrefetchStreamingURLLoaderStatus {
  // The streaming URL loader is in progress.
  kWaitingOnHead = 0,
  kHeadReceivedWaitingOnBody = 1,

  // The request redirected to a different target.
  kRedirected_DEPRECATED = 2,

  // Both the head and body of the response were received successfully.
  kSuccessfulNotServed = 3,
  kSuccessfulServedAfterCompletion = 4,
  kSuccessfulServedBeforeCompletion = 5,

  // Failure reasons based on the head of the response, corresponding to
  // `PrefetchErrorOnResponseReceived`.
  kPrefetchWasDecoy = 6,
  kFailedInvalidHead = 7,
  kFailedInvalidHeaders = 8,
  kFailedNon2XX = 9,
  kFailedMIMENotSupported = 10,

  // Failure reasons where the head of the response was good, but an error
  // occurred while receiving the body of the response.
  kFailedNetError = 11,
  kFailedNetErrorButServed = 12,

  // Statuses related to redirects.
  kFollowRedirect_DEPRECATED = 13,
  kPauseRedirectForEligibilityCheck_DEPRECATED = 14,
  kFailedInvalidRedirect = 15,
  kStopSwitchInNetworkContextForRedirect = 16,

  // The streaming URL loader was previously stopped after a redirect required a
  // change in network context, and is served.
  kServedSwitchInNetworkContextForRedirect = 17,

  kMaxValue = kServedSwitchInNetworkContextForRedirect,
};

enum class PrefetchRedirectStatus { kFollow, kFail, kSwitchNetworkContext };

using PrefetchRequestHandler = base::OnceCallback<void(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> forwarding_client)>;

// This callback is used by the owner to determine if the prefetch is valid
// based on |head|. If the prefetch should be servable based on |head|, then
// the callback should return `std::nullopt`. Otherwise it should return a
// failure reason.
enum class PrefetchErrorOnResponseReceived {
  kPrefetchWasDecoy,
  kFailedInvalidHead,
  kFailedInvalidHeaders,
  kFailedNon2XX,
  kFailedMIMENotSupported
};
using OnPrefetchResponseStartedCallback =
    base::OnceCallback<std::optional<PrefetchErrorOnResponseReceived>(
        network::mojom::URLResponseHead* head)>;

using OnPrefetchResponseCompletedCallback = base::OnceCallback<void(
    const network::URLLoaderCompletionStatus& completion_status)>;

// This callback is used by the owner to determine if the redirect should be
// followed. |HandleRedirect| should be called with the appropriate status for
// how the redirect should be handled.
using OnPrefetchRedirectCallback = base::RepeatingCallback<void(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr response_head)>;

// Design doc for ServiceWorker+Prefetch support:
// https://docs.google.com/document/d/1kbs8YJuh93F_K6JqW84YSsjVWcAeU41Mj9nwPr6IXRs/edit?usp=sharing
enum class PrefetchServiceWorkerState {
  // [Final state] All prefetch requests are ineligible for allowing
  // ServiceWorker interception. This is the existing behavior with the
  // feature flag disabled.
  //
  // By disabling ServiceWorker support, this state can support redirects,
  // isolated network contexts and proxies.
  //
  // During prefetch:
  // - The prefetch eligibility check fails (both for initial and redirected
  //   requests) when a controlling ServiceWorker is found.
  //
  // During navigation:
  // - `ServiceWorkerClient` is created (if needed) by
  //   `ServiceWorkerMainResourceLoaderInterceptor` (that is expected not to
  //   intercept the request because of no controlling ServiceWorker) and
  // - then the prefetch result is served by `PrefetchURLLoaderInterceptor`.
  kDisallowed,

  // The initial prefetch request is eligible for allowing ServiceWorker
  // interception.
  // Note: even in this state, all requests after redirects are ineligible for
  // ServiceWorker interception.
  //
  // During prefetch:
  // - The prefetch eligibility check for the initial request skips the
  // ServiceWorker check.
  // - Instead, the controlling ServiceWorker is looked up after
  // `PrefetchService::SendPrefetchRequest`is called, by
  // `ServiceWorkerMainResourceLoaderInterceptor` and its related classes.
  //   The `ServiceWorkerClient` is created here.
  // - At that time, if no controller ServiceWorkers are found, then fallback
  // to `kDisallowed`.
  //   This is to allow redirect support for same-origin requests that are
  //   eligible for ServiceWorker interception but actually aren't controlled
  //   by any ServiceWorkers.
  //   The created `ServiceWorkerClient` is discarded (which is *mostly* not
  //   observable because it isn't observed by fetch handlers), and another
  //   `ServiceWorkerClient` is created during navigaiton in the `kDisallowed`
  //   state.
  // - Otherwise, transition to `kControlled`.
  // - No responses/redirects are received in this state, because we
  // transition to another state before actually starting the URLLoaderFactory
  // of the initial prefetch request.
  //
  // During navigation:
  // - BlockUntilHead is not enabled in this state because it isn't
  // implemented yet in
  // `PrefetchURLLoaderInterceptorForServiceworkerControlled`.
  // - No actual serving occurs in this state because we don't receive the
  // response yet.
  kAllowed,

  // The prefetch request is intercepted by a controlling ServiceWorker and is
  // waiting for the initial request's response.
  // Note that the prefetch request can still fallback to the network (e.g.
  // ServiceWorker's fetch handler doesn't exist or doesn't call
  // respondWith()).
  //
  // During prefetch:
  // - All redirects are considered ineligible.
  //
  // During navigation:
  // - The prefetch result is served by
  // `PrefetchURLLoaderInterceptorForServiceworkerControlled`, passing the
  // URLLoaderFactory created by `ServiceWorkerMainResourceLoaderInterceptor`
  // and its related classes, and the `ServiceWorkerClient` created in the
  // `kEligible` state is above as the reserved client.
  kControlled,
};

using OnServiceWorkerStateDeterminedCallback =
    base::OnceCallback<void(PrefetchServiceWorkerState)>;

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_COMMON_TYPES_H_
