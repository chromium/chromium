// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_H_

#include <Foundation/Foundation.h>

#import "base/observer_list_types.h"

// Interface for listening to toolbars heights changes.
class ToolbarsSizeObserver : public base::CheckedObserver {
 public:
  ToolbarsSizeObserver(const ToolbarsSizeObserver&) = delete;
  ToolbarsSizeObserver& operator=(const ToolbarsSizeObserver&) = delete;
  ~ToolbarsSizeObserver() override;
  ToolbarsSizeObserver() = default;

  // Whenever the top toolbar height changes.
  virtual void OnTopToolbarHeightChanged() = 0;
  // Whenever the bottom toolbar height changes.
  virtual void OnBottomToolbarHeightChanged() = 0;
};

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUNDLED_FULLSCREEN_TOOLBARS_SIZE_OBSERVER_H_
