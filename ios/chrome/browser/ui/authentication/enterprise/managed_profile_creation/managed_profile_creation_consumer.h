// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_CONSUMER_H_

#import <UIKit/UIKit.h>

// Handles managed profile creation screen UI updates.
@protocol ManagedProfileCreationConsumer <NSObject>

@property(nonatomic, assign) BOOL canShowBrowsingDataMigration;

- (void)setKeepBrowsingDataSeparate:(BOOL)keepSeparate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_ENTERPRISE_MANAGED_PROFILE_CREATION_MANAGED_PROFILE_CREATION_CONSUMER_H_
