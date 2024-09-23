// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PASSWORD_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PASSWORD_CONSUMER_H_

#import <Foundation/Foundation.h>

@class ManualFillActionItem;
@class ManualFillCredentialItem;

// Objects conforming to this protocol need to react when new data is available.
@protocol ManualFillPasswordConsumer

// Tells the consumer to show the passed credentials.
- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials;

// Asks the consumer to present the passed actions
- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_MANUAL_FILL_PASSWORD_CONSUMER_H_
