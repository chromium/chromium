// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_CONSUMER_H_

// Provides display strings.
@protocol FamilyPromoConsumer <NSObject>

// Sets respective strings in the consumer.
- (void)setTitle:(NSString*)title subtitle:(NSString*)subtitle;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PASSWORD_PASSWORD_SHARING_FAMILY_PROMO_CONSUMER_H_
