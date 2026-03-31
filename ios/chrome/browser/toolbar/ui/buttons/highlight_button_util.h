// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_HIGHLIGHT_BUTTON_UTIL_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_HIGHLIGHT_BUTTON_UTIL_H_

#import <UIKit/UIKit.h>

// Returns a view with the IPH gradient background. The caller is responsible
// for adding it to their hierarchy and layout.
UIView* CreateIPHGradientView();

// Styles the imageView for IPH (white tint and drop shadow).
void ConfigureIPHImageStyleForImageView(UIImageView* imageView);

// Removes the IPH style from the imageView.
void RemoveIPHImageStyleFromImageView(UIImageView* imageView);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_HIGHLIGHT_BUTTON_UTIL_H_
