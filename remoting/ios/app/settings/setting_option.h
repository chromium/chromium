// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_SETTINGS_SETTING_OPTION_H_
#define REMOTING_IOS_APP_SETTINGS_SETTING_OPTION_H_

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, SettingOptionType) {
  OptionCheckbox,
  OptionSelector,
  FlatButton,
};

@interface SettingOption : NSObject

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* subtext;
@property(nonatomic, copy) void (^action)(void);
@property(nonatomic) BOOL checked;
@property(nonatomic) SettingOptionType style;

@end

#endif  // REMOTING_IOS_APP_SETTINGS_SETTING_OPTION_H_
