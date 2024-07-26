// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_HEADER_VIEW_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_HEADER_VIEW_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/home_customization/utils/home_customization_constants.h"

// The header for the customization pages describing the purpose of the current
// page.
@interface HomeCustomizationHeaderView : UICollectionReusableView

// The customization menu page that this header represents.
@property(nonatomic, assign) CustomizationMenuPage page;

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_HOME_CUSTOMIZATION_HEADER_VIEW_H_
