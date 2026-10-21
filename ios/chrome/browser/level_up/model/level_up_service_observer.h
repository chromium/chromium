// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_H_

#include "base/observer_list_types.h"

class LevelUpServiceObserver : public base::CheckedObserver {
 public:
  // Called when a task is completed.
  virtual void OnTaskCompleted() {}

  // Called when a level up stat value is updated.
  virtual void OnStatValueChanged() {}
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_OBSERVER_H_
