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

void TestShareKitService::ShareGroup(const TabGroup* group,
                                     UIViewController* base_view_controller,
                                     id<ApplicationCommands> commandsHandler) {
}

UIViewController* TestShareKitService::FacePile(NSString* collab_id) {
  // TODO(crbug.com/358373145): add fake implementation.
  return nil;
}

UIViewController* TestShareKitService::FacePile(
    ShareKitFacePileConfiguration* config) {
  // TODO(crbug.com/358373145): add fake implementation.
  return nil;
}
