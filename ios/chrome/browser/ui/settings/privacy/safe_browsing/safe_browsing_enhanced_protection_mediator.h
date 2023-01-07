// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol SafeBrowsingEnhancedProtectionConsumer;

// Mediator for the Safe Browsing Enhanced Protection UI.
@interface SafeBrowsingEnhancedProtectionMediator : NSObject

// Consumer for mediator.
@property(nonatomic, weak) id<SafeBrowsingEnhancedProtectionConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_SAFE_BROWSING_SAFE_BROWSING_ENHANCED_PROTECTION_MEDIATOR_H_
