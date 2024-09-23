// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSUMER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "base/values.h"

// Consumer for the Credential Provider Extension Promo. All of the setters
// should be called before -viewDidLoad is called.
@protocol CredentialProviderPromoConsumer

// Sets the respective state in the consumer. Must be called before -viewDidLoad
// is called.
- (void)setTitleString:(NSString*)titleString
           subtitleString:(NSString*)subtitleString
      primaryActionString:(NSString*)primaryActionString
    secondaryActionString:(NSString*)secondaryActionString
     tertiaryActionString:(NSString*)tertiaryActionString
                    image:(UIImage*)image;

// Passes the animation resource name to the consumer. Must be called before
// -viewDidLoad is called.
- (void)setAnimation:(NSString*)animationResourceName;

@end

#endif  // IOS_CHROME_BROWSER_CREDENTIAL_PROVIDER_PROMO_UI_BUNDLED_CREDENTIAL_PROVIDER_PROMO_CONSUMER_H_
