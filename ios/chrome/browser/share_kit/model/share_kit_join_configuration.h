// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "components/data_sharing/public/group_data.h"

@protocol ApplicationCommands;
enum class ShareKitFlowOutcome;
@class ShareKitPreviewItem;

// Configuration object for joining a shared group.
@interface ShareKitJoinConfiguration : NSObject

// The base view controller on which the join flow will be presented.
@property(nonatomic, weak) UIViewController* baseViewController;

// Application commands handler.
@property(nonatomic, weak) id<ApplicationCommands> applicationHandler;

// The token used to join the group, containing the collab ID and the secret.
@property(nonatomic, assign) data_sharing::GroupToken token;

// Executed when the join flow ended.
@property(nonatomic, copy) void (^completion)(ShareKitFlowOutcome outcome);

// The display name of the shared group.
@property(nonatomic, copy) NSString* displayName;

// The preview image to show in the Join screen.
@property(nonatomic, strong) UIImage *previewImage;

// The preview items to show in the preview screen.
@property(nonatomic, copy) NSArray<ShareKitPreviewItem*>* previewItems;

// Callback to be called when the collaboration group has been successfully
// joined. The callback parameter is to be called to dismiss the screen.
@property(nonatomic, copy) void (^joinCollaborationGroupSuccessBlock)
    (ProceduralBlock);

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_JOIN_CONFIGURATION_H_
