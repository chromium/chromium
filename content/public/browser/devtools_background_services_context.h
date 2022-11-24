// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_

#include <stdint.h>
#include <map>
#include <string>

#include "content/common/content_export.h"

namespace blink {
class StorageKey;
}  // namespace blink

namespace content {

enum class DevToolsBackgroundService {
  kBackgroundFetch = 2,
  kBackgroundSync = 3,
  kPushMessaging = 4,
  kNotifications = 5,
  kPaymentHandler = 6,
  kPeriodicBackgroundSync = 7,

  // Keep at the end.
  kMaxValue = kPeriodicBackgroundSync,
};

// This class is responsible for persisting the debugging events for the
// relevant Web Platform Features.
class CONTENT_EXPORT DevToolsBackgroundServicesContext {
 public:
  DevToolsBackgroundServicesContext() = default;

  DevToolsBackgroundServicesContext(const DevToolsBackgroundServicesContext&) =
      delete;
  DevToolsBackgroundServicesContext& operator=(
      const DevToolsBackgroundServicesContext&) = delete;

  virtual ~DevToolsBackgroundServicesContext() = default;

  // Whether events related to |service| should be recorded.
  virtual bool IsRecording(DevToolsBackgroundService service) = 0;

  // Logs the event if recording for |service| is enabled.
  // |storage_key| refers to the storage partition the event belongs to.
  // |event_name| is a description of the event.
  // |instance_id| is for tracking events related to the same feature instance.
  // Any additional useful information relating to the feature can be sent via
  // |event_metadata|. Called from the UI thread.
  virtual void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      blink::StorageKey storage_key,
      DevToolsBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
