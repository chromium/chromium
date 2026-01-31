// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

// App interface to interact with the Composebox.
@interface ComposeboxAppInterface : NSObject

// Sets the return value of MockIOSChromeAimEligibilityService::IsAimEligible.
+ (void)setAimEligible:(BOOL)eligible;

// Sets the return value of
// MockIOSChromeAimEligibilityService::IsCreateImagesEligible.
+ (void)setCreateImagesEligible:(BOOL)eligible;

// Sets the return value of
// MockIOSChromeAimEligibilityService::IsAimLocallyEligible.
+ (void)setAimLocallyEligible:(BOOL)eligible;

// Sets the return value of
// MockIOSChromeAimEligibilityService::IsServerEligibilityEnabled.
+ (void)setServerEligibilityEnabled:(BOOL)enabled;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_APP_INTERFACE_H_
