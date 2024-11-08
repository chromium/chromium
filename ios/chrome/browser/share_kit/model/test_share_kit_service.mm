// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"

#import "ios/chrome/browser/share_kit/model/fake_share_flow_view_controller.h"
#import "ios/chrome/browser/share_kit/model/share_kit_join_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_manage_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"

TestShareKitService::TestShareKitService() {}

TestShareKitService::~TestShareKitService() {}

bool TestShareKitService::IsSupported() const {
  return true;
}

void TestShareKitService::ShareGroup(ShareKitShareGroupConfiguration* config) {
  UIViewController* viewController = [[FakeShareFlowViewController alloc] init];
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

void TestShareKitService::ManageGroup(ShareKitManageConfiguration* config) {
  UIViewController* viewController = [[FakeShareFlowViewController alloc] init];
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

void TestShareKitService::JoinGroup(ShareKitJoinConfiguration* config) {
  UIViewController* viewController = [[FakeShareFlowViewController alloc] init];
  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  [config.baseViewController presentViewController:navController
                                          animated:YES
                                        completion:nil];
}

UIViewController* TestShareKitService::FacePile(
    ShareKitFacePileConfiguration* config) {
  return [[UIViewController alloc] init];
}

void TestShareKitService::ReadGroups(ShareKitReadConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

id<ShareKitAvatarPrimitive> TestShareKitService::AvatarImage(
    ShareKitAvatarConfiguration* config) {
  // TODO(crbug.com/375366568): add fake implementation.
  return nil;
}
