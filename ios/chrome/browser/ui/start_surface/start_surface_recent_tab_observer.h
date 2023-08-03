// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "base/observer_list_types.h"

namespace web {
class WebState;
}  // namespace web

// Interface for listening to updates to the most recent tab.
class StartSurfaceRecentTabObserver : public base::CheckedObserver {
 public:
  StartSurfaceRecentTabObserver() {}

  StartSurfaceRecentTabObserver(const StartSurfaceRecentTabObserver&) = delete;
  StartSurfaceRecentTabObserver& operator=(
      const StartSurfaceRecentTabObserver&) = delete;

  ~StartSurfaceRecentTabObserver() override;

  // Notifies the receiver that the most recent tab was removed.
  virtual void MostRecentTabRemoved(web::WebState* web_state) {}
  // Notifies the receiver that the favicon for the current page of the most
  // recent tab was updated to `image`.
  virtual void MostRecentTabFaviconUpdated(UIImage* image) {}

  virtual void MostRecentTabTitleUpdated(const std::u16string& title) {}
};

#endif  // IOS_CHROME_BROWSER_UI_START_SURFACE_START_SURFACE_RECENT_TAB_OBSERVER_H_
