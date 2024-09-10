// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_ITEM_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_ITEM_H_

#import <UIKit/UIKit.h>

@protocol SystemIdentity;

@interface SwitchProfileSettingsItem : NSObject

@property(nonatomic, strong) NSString* profileName;
@property(nonatomic, strong) UIImage* avatar;
@property(nonatomic, assign) BOOL active;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_MULTI_IDENTITY_SWITCH_PROFILE_SETTINGS_ITEM_H_
