// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

using ShareGroupCompletionBlock = void (^)(NSString* collabID);
using CompletionBlock = void (^)(BOOL result);

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

// Executed when the group is actually shared, after tapping the Save button.
// The collab ID that is returned is a new UUID.
@property(nonatomic, copy) ShareGroupCompletionBlock sharedGroupCompletionBlock;

// Executed when Cancel or Save are tapped. The `result` is then respectively
// NO or YES.
@property(nonatomic, copy) CompletionBlock completionBlock;

// Init the fake flow with a `type`.
- (instancetype)initWithType:(FakeShareKitFlowType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_FAKE_SHARE_KIT_FLOW_VIEW_CONTROLLER_H_
