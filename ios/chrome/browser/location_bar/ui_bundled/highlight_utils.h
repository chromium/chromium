// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_HIGHLIGHT_UTILS_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_HIGHLIGHT_UTILS_H_

#import <UIKit/UIKit.h>

// Returns a view with the IPH gradient background. The caller is responsible
// for adding it to their hierarchy and layout.
UIView* CreateIPHGradientView();

// Styles the imageView for IPH (white tint and drop shadow).
void ConfigureIPHImageStyleForImageView(UIImageView* imageView);

// Styles the button for IPH (white tint on the button and drop shadow on the
// image).
void ConfigureIPHImageStyleForButton(UIButton* button);

// Removes the IPH style from the imageView.
void RemoveIPHImageStyleFromImageView(UIImageView* imageView);

// Removes the IPH style from the button.
void RemoveIPHImageStyleFromButton(UIButton* button);

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_HIGHLIGHT_UTILS_H_
