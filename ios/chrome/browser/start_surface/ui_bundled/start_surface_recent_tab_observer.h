// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_OBSERVER_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_OBSERVER_H_

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

  // Notifies the receiver that the most recent tab (linked to `web_state`) was
  // removed.
  virtual void MostRecentTabRemoved(web::WebState* web_state) {}
  // Notifies the receiver that the favicon for the current page of the most
  // recent tab (linked to `web_state`) was updated to `image`.
  virtual void MostRecentTabFaviconUpdated(web::WebState* web_state,
                                           UIImage* image) {}
  // Notifies the receiver that the title for the current page of the most
  // recent tab (linked to `web_state`) was updated to `title`.
  virtual void MostRecentTabTitleUpdated(web::WebState* web_state,
                                         const std::u16string& title) {}
};

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_START_SURFACE_RECENT_TAB_OBSERVER_H_
