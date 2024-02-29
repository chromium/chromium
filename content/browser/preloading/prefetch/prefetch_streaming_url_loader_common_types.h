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

#endif  // CONTENT_BROWSER_PRELOADING_PREFETCH_PREFETCH_STREAMING_URL_LOADER_COMMON_TYPES_H_
