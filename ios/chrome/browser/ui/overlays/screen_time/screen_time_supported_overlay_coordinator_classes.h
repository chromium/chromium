// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_SCREEN_TIME_SCREEN_TIME_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_SCREEN_TIME_SCREEN_TIME_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_

#import <Foundation/Foundation.h>

namespace screen_time {

// Returns the supported OverlayRequestCoordinator classes for
// OverlayModality::kScreenTime.
NSArray<Class>* GetSupportedOverlayCoordinatorClasses();

}  // screen_time

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_SCREEN_TIME_SCREEN_TIME_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
