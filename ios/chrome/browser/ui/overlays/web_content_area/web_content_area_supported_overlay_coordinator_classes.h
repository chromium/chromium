// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_WEB_CONTENT_AREA_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
#define IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_WEB_CONTENT_AREA_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_

#import <Foundation/Foundation.h>

namespace web_content_area {

// Returns the supported OverlayRequestCoordinator classes for
// OverlayModality::kWebContentArea.
NSArray<Class>* GetSupportedOverlayCoordinatorClasses();

}  // web_content_area

#endif  // IOS_CHROME_BROWSER_UI_OVERLAYS_WEB_CONTENT_AREA_WEB_CONTENT_AREA_SUPPORTED_OVERLAY_COORDINATOR_CLASSES_H_
