// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_

#include <stdint.h>
#include <map>
#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"

namespace url {
class Origin;
}  // namespace url

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
  virtual ~DevToolsBackgroundServicesContext() = default;

  // Whether events related to |service| should be recorded.
  virtual bool IsRecording(DevToolsBackgroundService service) = 0;

  // Logs the event if recording for |service| is enabled.
  // |event_name| is a description of the event.
  // |instance_id| is for tracking events related to the same feature instance.
  // Any additional useful information relating to the feature can be sent via
  // |event_metadata|. Called from the UI thread.
  virtual void LogBackgroundServiceEvent(
      uint64_t service_worker_registration_id,
      const url::Origin& origin,
      DevToolsBackgroundService service,
      const std::string& event_name,
      const std::string& instance_id,
      const std::map<std::string, std::string>& event_metadata) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsBackgroundServicesContext);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_BACKGROUND_SERVICES_CONTEXT_H_
