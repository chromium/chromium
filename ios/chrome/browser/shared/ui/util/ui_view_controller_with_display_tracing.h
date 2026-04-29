// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_

#import <UIKit/UIKit.h>

// A mixin class that can be added to subclasses of UIViewController to inject
// new tracing capabilities based on CADisplayLink.
@interface UIViewControllerWithDisplayTracing : UIViewController

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_
