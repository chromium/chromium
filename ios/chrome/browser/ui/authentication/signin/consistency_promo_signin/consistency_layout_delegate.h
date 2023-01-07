// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_LAYOUT_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_LAYOUT_DELEGATE_H_

#import <UIKit/UIKit.h>

// Style to display the consistency sheet.
typedef NS_ENUM(NSUInteger, ConsistencySheetDisplayStyle) {
  // Bottom sheet at the bottom of the screen (for compact size).
  ConsistencySheetDisplayStyleBottom,
  // Bottom sheet centered in the middle of the screen (for regular size).
  ConsistencySheetDisplayStyleCentered,
};

@protocol ConsistencyLayoutDelegate <NSObject>

// Display style according to the trait collection.
@property(nonatomic, assign, readonly)
    ConsistencySheetDisplayStyle displayStyle;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_CONSISTENCY_PROMO_SIGNIN_CONSISTENCY_LAYOUT_DELEGATE_H_
