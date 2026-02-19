// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_
#define IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_

#import <UIKit/UIKit.h>

// Returns the color to be used for toolbar buttons.
UIColor* ToolbarButtonColor();

// Returns the color to be used for the location bar background in the toolbar,
// in incognito or not.
UIColor* ToolbarLocationBarBackgroundColor(bool incognito);

// Configures `button` to have the shadow of a toolbar button.
void ConfigureShadowForToolbarButton(UIView* button);

#endif  // IOS_CHROME_BROWSER_TOOLBAR_UI_BUTTONS_TOOLBAR_BUTTONS_UTILS_H_
