// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

// Observer for HomeBackgroundCustomizationService.
class HomeBackgroundCustomizationServiceObserver
    : public base::CheckedObserver {
 public:
  // Called when the background is changed.
  virtual void OnBackgroundChanged() = 0;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_OBSERVER_H_
