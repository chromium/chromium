// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Consumer for the Docking Promo. All of the setters should be called before
// -viewDidLoad is called.
@protocol DockingPromoConsumer

// Sets the respective state in the consumer. Must be called before -viewDidLoad
// is called.
- (void)setTitleString:(NSString*)titleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString
            animationName:(NSString*)animationName;

@end

#endif  // IOS_CHROME_BROWSER_DOCKING_PROMO_UI_DOCKING_PROMO_CONSUMER_H_
