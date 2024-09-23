// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TARGET_FRAME_CACHE_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TARGET_FRAME_CACHE_H_

#import <UIKit/UIKit.h>

#include <map>

// Stores target frames for a set of UIViews.
class TargetFrameCache {
 public:
  TargetFrameCache();
  ~TargetFrameCache();

  // Add or remove the target frame for a view.
  void AddFrame(UIView* view, CGRect rect);
  void RemoveFrame(UIView* view);

  // Gets the cached target frame for `view`.
  CGRect GetFrame(UIView* view);

  // Returns whether `view` has a cached target frame.
  bool HasFrame(UIView* view);

 private:
  std::map<UIView*, CGRect> targetFrames_;
};

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_TARGET_FRAME_CACHE_H_
