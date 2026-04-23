// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_

#import <UIKit/UIKit.h>

// Returns the color to be used for the buttons in the toolbar, in incognito or
// not.
UIColor* ToolbarElementBackgroundColor(bool incognito);

// Configures `button` to have the shadow of a toolbar button.
void ConfigureShadowForToolbarButton(UIView* button);

// Configures corner radius of the `container` so that it takes on a rounded
// rectangle shape if the window has a compact width. Otherwise, a a pill/circle
// shape.
void ConfigureCornerRadiusForToolbarButtonContainer(
    UIView* container,
    UITraitCollection* trait_collection);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_
