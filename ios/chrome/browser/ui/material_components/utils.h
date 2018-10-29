// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_UTILS_H_
#define IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_UTILS_H_

@class MDCAppBarViewController;
@class UIScrollView;

// Styles the passed app bar view controller for displaying in Settings,
// history, bookmarks, etc. It follows the card style for collection views.
void ConfigureAppBarViewControllerWithCardStyle(
    MDCAppBarViewController* viewController);

#endif  // IOS_CHROME_BROWSER_UI_MATERIAL_COMPONENTS_UTILS_H_
