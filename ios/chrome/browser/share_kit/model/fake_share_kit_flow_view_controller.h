// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

enum class ShareKitFlowOutcome;

using ShareGroupCompletionBlock = void (^)(NSString* collabID,
                                           ProceduralBlock continuationBlock);
using CompletionBlock = void (^)(ShareKitFlowOutcome result);

// Different types of fake share kit flows.
enum class FakeShareKitFlowType {
  // Faking a "Share" flow.
  kShare,
  // Faking a "Manage" flow.
  kManage,
  // Faking a "Join" flow.
  kJoin,
};

// The view controllers presented by the TestShareKitService.
// It features a Cancel and a Save bar button items.
@interface FakeShareKitFlowViewController : UIViewController

// Executed when the group is actually shared or joined, after tapping the Save
// button. The collab ID that is returned is a new UUID.
@property(nonatomic, copy) ShareGroupCompletionBlock actionAcceptedBlock;

// Executed when the flow terminates (after accepting or cancelling).
@property(nonatomic, copy) CompletionBlock flowCompleteBlock;

// Init the fake flow with a `type`.
- (instancetype)initWithType:(FakeShareKitFlowType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Simulates the user tapping on "save", to be used in unit tests.
- (void)accept;
// Simulates the user tapping on "cancel", to be used in unit tests.
- (void)cancel;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_
