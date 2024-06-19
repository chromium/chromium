// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_CONSUMER_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_CONSUMER_H_

#import <Foundation/Foundation.h>

// Protocol to handle the UI behaviors of the autofill branding.
@protocol BrandingConsumer

// Whether the branding icon should be visible next time the keyboard pops up.
@property(nonatomic, assign) BOOL visible;

// Whether the branding icon should perform animation next time the branding
// icon shows up.
@property(nonatomic, assign) BOOL shouldPerformPopAnimation;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_BRANDING_BRANDING_CONSUMER_H_
