// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_TESTING_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_TESTING_H_

#import "ios/chrome/browser/settings/ui_bundled/privacy/privacy_safe_browsing_mediator.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"

// Testing category used for tests only.
@interface PrivacySafeBrowsingMediator (Testing)
- (void)updatePrivacySafeBrowsingSectionAndNotifyConsumer:(BOOL)notifyConsumer;
- (BOOL)shouldItemTypeHaveCheckmark:(NSInteger)itemType;
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingEnhancedProtectionPreference;
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingStandardProtectionPreference;
@property(nonatomic, strong, readwrite)
    NSArray<TableViewItem*>* safeBrowsingItems;
@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_PRIVACY_PRIVACY_SAFE_BROWSING_MEDIATOR_TESTING_H_
