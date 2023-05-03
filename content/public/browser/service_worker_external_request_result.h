// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_RESULT_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_RESULT_H_

namespace content {

// The result of adjusting the ref count of a service worker by an embedder,
// which keeps the service worker running.
// This enum is used in UMA histograms. Append-only. Keep in sync with
// tools/metrics/histograms/enums.xml version.
enum class ServiceWorkerExternalRequestResult {
  // The ref count was adjusted successfully.
  kOk = 0,

  // Error cases (the ref count did not change):
  // There is already an outstanding request with the given id (if
  // incrementing), or there was not an outstanding request with that id (if
  // decrementing).
  kBadRequestId = 1,
  // The worker is already stopping or stopped (if incrementing), or
  // is already stopped (if decrementing).q
  kWorkerNotRunning = 2,
  // The worker with the given version id doesn't exist or is not live.
  kWorkerNotFound = 3,
  // The core context inside ServiceWorkerContext has been destroyed.
  // This can happen during init or shutdown or a fatal storage error
  // occurred and the context is being reinitialized.
  kNullContext = 4,

  // Add new status codes here and update kMaxValue. The next new status code
  // should be 5.

  // Note: kMaxValue is needed only for histograms.
  kMaxValue = kNullContext,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_RESULT_H_
