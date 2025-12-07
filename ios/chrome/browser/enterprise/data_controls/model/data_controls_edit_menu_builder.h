// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_EDIT_MENU_BUILDER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_EDIT_MENU_BUILDER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/browser_container/model/edit_menu_builder.h"

// A builder that modifies the edit menu based on Data Controls policies.
@interface DataControlsEditMenuBuilder : NSObject <EditMenuBuilder>
@end

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_MODEL_DATA_CONTROLS_EDIT_MENU_BUILDER_H_
