// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_

#import <UIKit/UIKit.h>

// Bitmask for selecting which UI update steps are traced.
typedef NS_OPTIONS(NSUInteger, UIViewControllerDisplayTracingOptions) {
  UIViewControllerDisplayTracingOptionNone = 0,
  UIViewControllerDisplayTracingOptionEventDispatch = 1 << 0,
  UIViewControllerDisplayTracingOptionCADisplayLinkDispatch = 1 << 1,
  UIViewControllerDisplayTracingOptionCATransactionCommit = 1 << 2,
  UIViewControllerDisplayTracingOptionLayout = 1 << 3,
  UIViewControllerDisplayTracingOptionAppear = 1 << 4,

  UIViewControllerDisplayTracingOptionAllTraces =
      UIViewControllerDisplayTracingOptionEventDispatch |
      UIViewControllerDisplayTracingOptionCADisplayLinkDispatch |
      UIViewControllerDisplayTracingOptionCATransactionCommit |
      UIViewControllerDisplayTracingOptionLayout |
      UIViewControllerDisplayTracingOptionAppear,

  UIViewControllerDisplayTracingOptionEssentialTraces =
      UIViewControllerDisplayTracingOptionCATransactionCommit |
      UIViewControllerDisplayTracingOptionLayout |
      UIViewControllerDisplayTracingOptionAppear,
};

// A mixin class that can be added to subclasses of UIViewController to inject
// new tracing capabilities based on CADisplayLink.
@interface UIViewControllerWithDisplayTracing : UIViewController

// Initializes the view controller with specific display tracing options.
- (instancetype)initWithDisplayTracingOptions:
    (UIViewControllerDisplayTracingOptions)displayTracingOptions;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil
          displayTracingOptions:
              (UIViewControllerDisplayTracingOptions)displayTracingOptions;

- (instancetype)initWithCoder:(NSCoder*)coder
        displayTracingOptions:
            (UIViewControllerDisplayTracingOptions)displayTracingOptions;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_UI_VIEW_CONTROLLER_WITH_DISPLAY_TRACING_H_
