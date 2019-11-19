// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FEATURES_H_

#import <Foundation/Foundation.h>

#include "base/feature_list.h"

extern const base::Feature kToolbarNewTabButton;

// Enum for the different icons for the search button.
typedef NS_ENUM(NSUInteger, ToolbarSearchButtonIcon) {
  ToolbarSearchButtonIconGrey,
  ToolbarSearchButtonIconColorful,
  ToolbarSearchButtonIconMagnifying,
};

// Feature for changing the different icons for search icon in the bottom
// toolbar.
extern const base::Feature kIconForSearchButtonFeature;

// Switch with the different values for the search icon on the bottom adaptive
// toolbar.
extern const char kIconForSearchButtonFeatureParameterName[];
extern const char kIconForSearchButtonParameterGrey[];
extern const char kIconForSearchButtonParameterColorful[];
extern const char kIconForSearchButtonParameterMagnifying[];

// Returns the currently selected icon option.
ToolbarSearchButtonIcon IconForSearchButton();

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_PUBLIC_FEATURES_H_
