// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_H_

#include <Foundation/Foundation.h>

#import "base/observer_list_types.h"

// Interface for listening to toolbars heights changes.
class ToolbarUIObserver : public base::CheckedObserver {
 public:
  ToolbarUIObserver(const ToolbarUIObserver&) = delete;
  ToolbarUIObserver& operator=(const ToolbarUIObserver&) = delete;
  ~ToolbarUIObserver() override;
  ToolbarUIObserver() = default;

  // Whenever the top toolbar height changes.
  virtual void OnTopToolbarHeightChanged() = 0;
  // Whenever the bottom toolbar height changes.
  virtual void OnBottomToolbarHeightChanged() = 0;
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBAR_UI_OBSERVER_H_
