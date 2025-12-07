// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
enum class ShareKitFlowOutcome;
class TabGroup;

// Configuration object for the ShareKit ShareGroup API.
@interface ShareKitShareGroupConfiguration : NSObject

// The base view controller from which to present UI.
@property(nonatomic, weak) UIViewController* baseViewController;

// Local tab group.
@property(nonatomic, assign) const TabGroup* tabGroup;

// The group image preview.
@property(nonatomic, copy) UIImage* groupImage;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// Executed when the share flow ended.
@property(nonatomic, copy) void (^completion)(ShareKitFlowOutcome outcome);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SHARE_GROUP_CONFIGURATION_H_
