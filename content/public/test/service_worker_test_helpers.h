// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_
#define CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_

#include "base/functional/callback_forward.h"
#include "base/test/simple_test_tick_clock.h"

class GURL;

namespace blink {
struct PlatformNotificationData;
}  // namespace blink

namespace content {

class ServiceWorkerContext;

// Stops the active service worker of the registration for the given |scope|,
// and calls |complete_callback_ui| callback on UI thread when done.
//
// Can be called from UI/IO thread.
void StopServiceWorkerForScope(ServiceWorkerContext* context,
                               const GURL& scope,
                               base::OnceClosure complete_callback_ui);

// Dispatches a notification click event to the active service worker
// worker for the given |scope|.
void DispatchServiceWorkerNotificationClick(
    ServiceWorkerContext* context,
    const GURL& scope,
    const blink::PlatformNotificationData& notification_data);

// Advance the clock of a service worker after the timeout for requests.
void AdvanceClockAfterRequestTimeout(ServiceWorkerContext* context,
                                     int64_t service_worker_version_id,
                                     base::SimpleTestTickClock* tick_clock);

// Runs the user tasks on a service worker, triggers a timeout and returns
// whether the service worker is still running.
bool TriggerTimeoutAndCheckRunningState(ServiceWorkerContext* context,
                                        int64_t service_worker_version_id);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_
