// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;

// Configuration object for managing a shared group.
@interface ShareKitManageConfiguration : NSObject

// The base view controller on which the manage flow will be presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The collaboration ID of the shared tab group.
@property(nonatomic, copy) NSString* collabID;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_
