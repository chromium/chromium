// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_H_

#include "base/observer_list_types.h"

namespace web {
class WebState;
}

class DataProtectionTabHelper;

// Interface for observing data protection changes in a Tab.
class DataProtectionTabHelperObserver : public base::CheckedObserver {
 public:
  // Called when the screenshot protection state changes.
  virtual void ScreenshotProtectionDidChange(
      web::WebState* web_state,
      bool screenshot_protection_enabled) {}

  // Called when the DataProtectionTabHelper is destroyed. Observers should stop
  // observing.
  virtual void DataProtectionTabHelperDestroyed(
      DataProtectionTabHelper* helper) {}
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_PROTECTION_MODEL_DATA_PROTECTION_TAB_HELPER_OBSERVER_H_
