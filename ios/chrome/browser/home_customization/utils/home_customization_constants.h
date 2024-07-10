// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/shared/ui/list_model/list_model.h"

// The identifier for the bottom sheet's initial detent.
extern NSString* const kBottomSheetDetentIdentifier;

// Enum representing the customization submenus that can be navigated to.
enum class CustomizationMenuPage : NSInteger {
  kCustomizationMenuPageMain,
  kCustomizationMenuPageMagicStack,
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UTILS_HOME_CUSTOMIZATION_CONSTANTS_H_
