// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_UTILS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_UTILS_H_

#import <UIKit/UIKit.h>

// Returns true if the top toolbar should have full height.
bool ShouldHaveFullHeightTopToolbar(id<UITraitEnvironment> trait_environment);

// Returns true if the toolbar should have a compact location bar.
bool ShouldHaveCompactLocationBar(UITraitCollection* trait_collection);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_TOOLBAR_UTILS_H_
