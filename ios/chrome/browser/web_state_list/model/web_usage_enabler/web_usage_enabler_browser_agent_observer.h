// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_H_
#define IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_H_

#include "base/observer_list_types.h"

class WebUsageEnablerBrowserAgent;

// Observer interface for objects interested in WebUsageEnabler events.
class WebUsageEnablerBrowserAgentObserver : public base::CheckedObserver {
 public:
  WebUsageEnablerBrowserAgentObserver(
      const WebUsageEnablerBrowserAgentObserver&) = delete;
  WebUsageEnablerBrowserAgentObserver& operator=(
      const WebUsageEnablerBrowserAgentObserver&) = delete;

  virtual void WebUsageEnablerValueChanged(
      WebUsageEnablerBrowserAgent* web_usage_enabler) {}

 protected:
  WebUsageEnablerBrowserAgentObserver() = default;
};

#endif  // IOS_CHROME_BROWSER_WEB_STATE_LIST_MODEL_WEB_USAGE_ENABLER_WEB_USAGE_ENABLER_BROWSER_AGENT_OBSERVER_H_
