// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_TOP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_TOP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace top_view_controller {

// DEPRECATED -- do not add further usage of these functions.
// TODO(crbug.com/40534720): Remove TopPresentedViewControllerFrom().
UIViewController* TopPresentedViewControllerFrom(
    UIViewController* base_view_controller);

// TODO(crbug.com/40534720): Remove TopPresentedViewController().
UIViewController* TopPresentedViewController();

}  // namespace top_view_controller

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_TOP_VIEW_CONTROLLER_H_
