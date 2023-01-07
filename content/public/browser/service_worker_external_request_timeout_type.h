// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_TIMEOUT_TYPE_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_TIMEOUT_TYPE_H_

namespace content {

// The timeout types for ServiceWorker external requests via
// ServiceWorkerContext::StartingExternalRequest().
enum class ServiceWorkerExternalRequestTimeoutType {
  // Default timeout: kRequestTimeout.
  kDefault,
  // Timeout indicating that SW won't time out before
  // ServiceWorkerContext::FinishedExternalRequest() is issued.
  kDoesNotTimeout,
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_EXTERNAL_REQUEST_TIMEOUT_TYPE_H_
