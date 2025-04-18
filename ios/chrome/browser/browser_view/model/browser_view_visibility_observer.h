// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_H_
#define IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_H_

#import "base/observer_list_types.h"
#import "ios/chrome/browser/browser_view/public/browser_view_visibility_state.h"

/// Observer used to update listeners of change of browser visibility
class BrowserViewVisibilityObserver : public base::CheckedObserver {
 public:
  BrowserViewVisibilityObserver(const BrowserViewVisibilityObserver&) = delete;
  BrowserViewVisibilityObserver& operator=(
      const BrowserViewVisibilityObserver&) = delete;

  ~BrowserViewVisibilityObserver() override;

  /// Browser visibility has changed from `previous_state` to `current_state`.
  virtual void BrowserViewVisibilityStateDidChange(
      BrowserViewVisibilityState current_state,
      BrowserViewVisibilityState previous_state) {}

 protected:
  BrowserViewVisibilityObserver();
};

#endif  // IOS_CHROME_BROWSER_BROWSER_VIEW_MODEL_BROWSER_VIEW_VISIBILITY_OBSERVER_H_
