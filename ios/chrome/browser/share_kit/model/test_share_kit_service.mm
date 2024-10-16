// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"

TestShareKitService::TestShareKitService() {}
TestShareKitService::~TestShareKitService() {}

bool TestShareKitService::IsSupported() const {
  return true;
}

void TestShareKitService::ShareGroup(const TabGroup* group,
                                     UIViewController* base_view_controller) {
  // TODO(crbug.com/358373145): add fake implementation.
}
