// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"

TestShareKitService::TestShareKitService() {}

TestShareKitService::~TestShareKitService() {}

bool TestShareKitService::IsSupported() const {
  return true;
}

void TestShareKitService::ShareGroup(ShareKitShareGroupConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

void TestShareKitService::ManageGroup(ShareKitManageConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

void TestShareKitService::JoinGroup(ShareKitJoinConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}

UIViewController* TestShareKitService::FacePile(
    ShareKitFacePileConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
  return nil;
}

void TestShareKitService::ReadGroups(ShareKitReadConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
}
