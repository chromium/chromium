// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/http_error_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/common/content_client.h"

namespace content {

// static
std::unique_ptr<NavigationThrottle>
HttpErrorNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle& navigation_handle) {
  // We only care about primary main frame navigations.
  if (!navigation_handle.IsInPrimaryMainFrame())
    return nullptr;
  return base::WrapUnique(new HttpErrorNavigationThrottle(navigation_handle));
}

HttpErrorNavigationThrottle::HttpErrorNavigationThrottle(
    NavigationHandle& navigation_handle)
    : NavigationThrottle(&navigation_handle),
      task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()),
      body_consumer_watcher_(FROM_HERE,
                             mojo::SimpleWatcher::ArmingPolicy::MANUAL,
                             task_runner_) {}

const char* HttpErrorNavigationThrottle::GetNameForLogging() {
  return "HttpErrorNavigationThrottle";
}

HttpErrorNavigationThrottle::~HttpErrorNavigationThrottle() = default;

NavigationThrottle::ThrottleCheckResult
HttpErrorNavigationThrottle::WillProcessResponse() {
  // We've received the response head, but the response body might not be
  // readable yet. We should wait for the body, but only if the response might
  // result in an error page (response_code indicates an error, and the embedder
  // has a custom error page for it).
  const network::mojom::URLResponseHead* response =
      NavigationRequest::From(navigation_handle())->response();
  DCHECK(response);
  if (!response->headers)
    return PROCEED;
  int response_code = response->headers->response_code();
  if (response_code < 400 ||
      !GetContentClient()->browser()->HasErrorPage(response_code)) {
    return PROCEED;
  }

  // Defer the navigation until we can determine if the response body is empty
  // or not, by waiting until it becomes readable or the connection is closed.
  body_consumer_watcher_.Watch(
      NavigationRequest::From(navigation_handle())->response_body(),
      MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
      base::BindRepeating(&HttpErrorNavigationThrottle::OnBodyReadable,
                          base::Unretained(this)));
  body_consumer_watcher_.ArmOrNotify();
  return DEFER;
}

void HttpErrorNavigationThrottle::OnBodyReadable(MojoResult) {
  const mojo::DataPipeConsumerHandle& body =
      NavigationRequest::From(navigation_handle())->response_body();
  // See how many bytes are in the body, without consuming anything from the
  // response body data pipe.
  size_t num_bytes = 0;
  MojoResult result = body.ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                    base::span<uint8_t>(), num_bytes);

  switch (result) {
    case MOJO_RESULT_OK:
      break;
    case MOJO_RESULT_FAILED_PRECONDITION:
      // Failed reading the result, can be due to the connection being closed.
      // We should treat this as a signal that the body is empty.
      DCHECK_EQ(num_bytes, 0u);
      break;
    case MOJO_RESULT_SHOULD_WAIT:
      // Wait for the next signal to try and read the body again.
      body_consumer_watcher_.ArmOrNotify();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
      return;
  }

  // Stop watching for signals.
  body_consumer_watcher_.Cancel();

  if (num_bytes == 0) {
    // The response body is empty, so cancel the navigation and commit an error
    // page instead. The error page's content will be generated in the renderer
    // at commit time, so we only need to pass the error code in the call below.
    CancelDeferredNavigation({content::NavigationThrottle::CANCEL,
                              net::ERR_HTTP_RESPONSE_CODE_FAILURE});
  } else {
    // There's at least one byte in the body, which means it's not empty. The
    // navigation should continue normally.
    Resume();
  }
}

}  // namespace content
