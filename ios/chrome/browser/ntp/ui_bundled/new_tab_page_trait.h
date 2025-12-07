// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_TRAIT_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_TRAIT_H_

#import <UIKit/UITraitCollection.h>

#import "ios/chrome/browser/shared/ui/util/custom_ui_trait_accessor.h"

@class NewTabPageColorPalette;

// A trait definition for the New Tab Page (NTP).
@interface NewTabPageTrait : NSObject <UIObjectTraitDefinition>

@end

@interface CustomUITraitAccessor (NewTabPageTrait)

- (void)setObjectForNewTabPageTrait:(NewTabPageColorPalette*)object;

- (NewTabPageColorPalette*)objectForNewTabPageTrait;

@end

@interface UITraitCollection (NewTabPageTrait)

- (NewTabPageColorPalette*)objectForNewTabPageTrait;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_TRAIT_H_
