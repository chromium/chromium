// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PREFETCH_REQUEST_STATUS_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_PREFETCH_REQUEST_STATUS_LISTENER_H_

namespace content {

// Callback interface to receive the status of a prefetch request.
// The callback lifecycle is as follows:
//
// 1. If the prefetch request fails to start (i.e. before the URL loader is
// created & started) for any reason
// |PrefetchStatusListener::OnPrefetchStartFailed()| is called. This is a
// terminal signal (i.e. no more callback signals will emit for "this" prefetch
// request).
//
// 2. After the prefetch request starts (i.e. the URL loader is created and
// started) one of the following callbacks will be called (mutually exclusive):
//
// 2a. |PrefetchStatusListener::OnPrefetchResponseCompleted()| if the response
// was fetched successfully or has been served to a navigation (after all
// redirects).
//
// 2b. |PrefetchStatusListener::OnPrefetchResponseError()| if the server
// responds with a non 2XX response code (after all redirects).
//
// 2c. |PrefetchStatusListener::OnPrefetchResponseError()| if any other error
// occurs during the response fetching (after all redirects).
class PrefetchRequestStatusListener {
 public:
  virtual ~PrefetchRequestStatusListener() = default;

  // Called when the prefetch request failed to start.
  virtual void OnPrefetchStartFailedGeneric() = 0;

  // Called when an outgoing prefetch request is deemed to be a duplicate
  // (based on prefetch cache heuristics) and will therefore not be sent.
  virtual void OnPrefetchStartFailedDuplicate() = 0;

  // Called when the prefetch response has been fetched.
  // This is only emitted for the final response (i.e. after all redirects).
  virtual void OnPrefetchResponseCompleted() = 0;

  // Called when there is an error with the prefetch response that is not server
  // related.
  virtual void OnPrefetchResponseError() = 0;

  // Called when the server returns an error status code for a prefetch request
  // (e.g. 500). The |response_code| is the HTTP response code returned from
  // the server.
  virtual void OnPrefetchResponseServerError(int response_code) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PREFETCH_REQUEST_STATUS_LISTENER_H_
