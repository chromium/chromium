// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
class TabGroup;

// Configuration object for the ShareKit ShareGroup API.
@interface ShareKitShareGroupConfiguration : NSObject

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// Local tab group.
@property(nonatomic, assign) const TabGroup* tabGroup;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_
