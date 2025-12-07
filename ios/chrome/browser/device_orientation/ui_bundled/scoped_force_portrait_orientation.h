// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_SCOPED_FORCE_PORTRAIT_ORIENTATION_H_
#define IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_SCOPED_FORCE_PORTRAIT_ORIENTATION_H_

#import <Foundation/Foundation.h>

#include <memory>

@protocol PortraitOrientationManager;

// A scoped object using `manager` to force the UI in portrait orientation
// until it is destroyed.
class ScopedForcePortraitOrientation {
 public:
  explicit ScopedForcePortraitOrientation(
      id<PortraitOrientationManager> manager);

  ScopedForcePortraitOrientation(const ScopedForcePortraitOrientation&) =
      delete;
  ScopedForcePortraitOrientation& operator=(
      const ScopedForcePortraitOrientation&) = delete;

  ~ScopedForcePortraitOrientation();

 private:
  // The target blocking the portrait only.
  __weak id<PortraitOrientationManager> manager_;
};

// Returns a ScopedForcePortraitOrientation on iPhone or null.
std::unique_ptr<ScopedForcePortraitOrientation>
ForcePortraitOrientationOnIphone(id<PortraitOrientationManager> manager);

#endif  // IOS_CHROME_BROWSER_UI_DEVICE_ORIENTATION_SCOPED_FORCE_PORTRAIT_ORIENTATION_H_
