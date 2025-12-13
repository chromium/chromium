// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
enum class ShareKitFlowOutcome;
class TabGroup;

typedef void (^ShareKitShouldUnshareGroupBlock)(BOOL shouldDelete);

// Configuration object for managing a shared group.
@interface ShareKitManageConfiguration : NSObject

// The base view controller on which the manage flow will be presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// The collaboration ID of the shared tab group.
@property(nonatomic, copy) NSString* collabID;

// Local tab group.
@property(nonatomic, assign) const TabGroup* tabGroup;

// The group image preview.
@property(nonatomic, copy) UIImage* groupImage;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// Whether enterprise sharing is disabled.
@property(nonatomic, assign) BOOL enterpriseSharingDisabled;

// Executed when the manage flow ended.
@property(nonatomic, copy) void (^completion)(ShareKitFlowOutcome outcome);

// The completion block to be called when the user requests to delete the group,
// to know if the deletion should proceed or not, providing an opportunity to
// act before the collaboration group is deleted.
@property(nonatomic, copy) void (^willUnshareGroupBlock)
    (ShareKitShouldUnshareGroupBlock continuationBlock);

// The completion block to be called when the collaboration group has been
// successfully deleted.
@property(nonatomic, copy) void (^didUnshareGroupBlock)(NSError* error);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_MANAGE_CONFIGURATION_H_
