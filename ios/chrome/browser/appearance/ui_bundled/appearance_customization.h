// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APPEARANCE_UI_BUNDLED_APPEARANCE_CUSTOMIZATION_H_
#define IOS_CHROME_BROWSER_APPEARANCE_UI_BUNDLED_APPEARANCE_CUSTOMIZATION_H_

#import <UIKit/UIKit.h>

// Overrides some default appearance values for UIKit controls.
void CustomizeUIAppearance();

// Overrides some default UIWindow appearance values.
void CustomizeUIWindowAppearance(UIWindow* window);

#endif  // IOS_CHROME_BROWSER_APPEARANCE_UI_BUNDLED_APPEARANCE_CUSTOMIZATION_H_
