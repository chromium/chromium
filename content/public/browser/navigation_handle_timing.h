// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_

#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// NavigationHandleTiming contains timing information of loading for navigation
// recorded in NavigationHandle. This is used for UMAs, not exposed to
// JavaScript via Navigation Timing API etc unlike mojom::NavigationTiming. See
// the design doc for details.
// https://docs.google.com/document/d/16oqu9lyPbfgZIjQsRaCfaKE8r1Cdlb3d4GVSdth4AN8/edit?usp=sharing
struct CONTENT_EXPORT NavigationHandleTiming {
  NavigationHandleTiming();
  NavigationHandleTiming(const NavigationHandleTiming& timing);
  NavigationHandleTiming& operator=(const NavigationHandleTiming& timing);

  // The time the URLLoader for the navigation started.
  base::TimeTicks loader_start_time;

  // The time the first HTTP request was sent. This is filled with
  // net::LoadTimingInfo::send_start during navigation.
  //
  // In some cases, this can be the time an internal request started that did
  // not go to the networking layer. For example,
  // - Service Worker: the time the fetch event was ready to be dispatched, see
  //   content::ServiceWorkerMainResourceLoader::DidPrepareFetchEvent()).
  // - HSTS: the time the internal redirect was handled.
  // - Signed Exchange: the time the SXG was handled.
  base::TimeTicks first_request_start_time;

  // The time the headers of the first HTTP response were received. This is
  // filled with net::LoadTimingInfo::receive_headers_start on the first HTTP
  // response during navigation. The response can be informational (1xx).
  //
  // In some cases, this can be the time an internal response was received that
  // did not come from the networking layer. For example,
  // - Service Worker: the time the response from the service worker was
  //   received, see content::ServiceWorkerMainResourceLoader::StartResponse().
  // - HSTS: the time the internal redirect was handled.
  // - Signed Exchange: the time the SXG was handled.
  base::TimeTicks first_response_start_time;

  // The time a callback for the navigation loader was first invoked. The time
  // between this and |first_response_start_time| includes any throttling or
  // process/thread hopping between the network stack receiving the response and
  // the navigation loader receiving it.
  base::TimeTicks first_loader_callback_time;

  // The time the final HTTP request was sent. This is filled with
  // net::LoadTimingInfo::send_start during navigation.
  //
  // Note that if this value is checked before the navigation received the
  // final non-redirect response, this might be set to a "non-final" request
  // time, which can still result in a redirect. This is because the value is
  // updated every time we get a response, which can be a redirect response. See
  // also `non_redirected_request_start_time`.
  //
  // In some cases, this can be the time an internal request started that did
  // not go to the networking layer. See the comment for
  // |first_request_start_time|.
  //
  // This is equal to |first_request_start_time| if there is no redirection.
  //
  // TODO(https://crbug.com/347706997): Consider renaming or not setting this
  // until we get a non-redirect response, to avoid confusion.
  base::TimeTicks final_request_start_time;

  // The time the headers of the final HTTP response were received. This is
  // filled with net::LoadTimingInfo::receive_headers_start on the final HTTP
  // response during navigation. The response can be informational (1xx).
  //
  // Note that if this value is checked before the navigation received the
  // final non-redirect response, this might be set to a "non-final" redirect
  // response time. This is because the value is updated every time we get a
  // response, which can be a redirect response. See also
  // `non_redirect_response_start_time`.
  //
  // In some cases, this can be the time an internal response was received that
  // did not come from the networking layer. See the comment for
  // |first_response_start_time|.
  //
  // This is equal to |first_response_start_time| if there is no redirection.
  //
  // TODO(https://crbug.com/347706997): Consider renaming or not setting this
  // until we get a non-redirect response, to avoid confusion.
  base::TimeTicks final_response_start_time;

  // Similar to `final_request_start_time`, `final_response_start_time`, and
  // `first_loader_callback_time`, but only set when we get the final
  // non-redirect response for the navigation.
  base::TimeTicks non_redirected_request_start_time;
  base::TimeTicks non_redirect_response_start_time;
  base::TimeTicks non_redirect_response_loader_callback_time;

  // The time the headers of the final non-informational (non-1xx) HTTP response
  // were received. This is filled with
  // net::LoadTimingInfo::receive_non_informational_headers_start on the final
  // non-informational HTTP response during navigation. If no informational
  // responses are received, this is equal to |final_response_start_time|.
  base::TimeTicks final_non_informational_response_start_time;

  // The time a callback for the navigation loader was last invoked. The time
  // between this and |final_response_start_time| includes any throttling or
  // process/thread hopping between the network stack receiving the response and
  // the navigation loader receiving it.
  //
  // This is equal to |first_loader_callback_time| if there is no redirection.
  base::TimeTicks final_loader_callback_time;

  // The time the navigation request is determined to be failed and turned to
  // commit an error page.
  base::TimeTicks request_failed_time;

  // The time the navigation commit message was sent to a renderer process.
  base::TimeTicks navigation_commit_sent_time;

  // The time the navigation commit message was received in the renderer
  // process.
  base::TimeTicks navigation_commit_received_time;

  // The time the DidCommit navigation message was received in the browser
  // process.
  base::TimeTicks navigation_did_commit_time;

  // ConnectTiming related delay information for the first HTTP response.
  base::TimeDelta first_request_domain_lookup_delay;
  base::TimeDelta first_request_connect_delay;
  base::TimeDelta first_request_ssl_delay;

  // ConnectTiming related delay information for the final HTTP response.
  base::TimeDelta final_request_domain_lookup_delay;
  base::TimeDelta final_request_connect_delay;
  base::TimeDelta final_request_ssl_delay;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_TIMING_H_
