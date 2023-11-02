// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_APP_SETTINGS_SETTINGS_VIEW_CELL_H_
#define REMOTING_IOS_APP_SETTINGS_SETTINGS_VIEW_CELL_H_

#import <UIKit/UIKit.h>

#import <MaterialComponents/MaterialCollections.h>

#import "remoting/ios/app/settings/setting_option.h"

@interface SettingsViewCell : MDCCollectionViewCell
- (void)setSettingOption:(SettingOption*)option;
@end

#endif  // REMOTING_IOS_APP_SETTINGS_SETTINGS_VIEW_CELL_H_
