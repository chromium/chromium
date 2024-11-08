// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

// Test implementation of the ShareKitService.
class TestShareKitService : public ShareKitService {
 public:
  TestShareKitService();
  TestShareKitService(const TestShareKitService&) = delete;
  TestShareKitService& operator=(const TestShareKitService&) = delete;
  ~TestShareKitService() override;

  // ShareKitService.
  bool IsSupported() const override;
  void ShareGroup(ShareKitShareGroupConfiguration* config) override;
  void ManageGroup(ShareKitManageConfiguration* config) override;
  void JoinGroup(ShareKitJoinConfiguration* config) override;
  UIViewController* FacePile(ShareKitFacePileConfiguration* config) override;
  void ReadGroups(ShareKitReadConfiguration* config) override;
  id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config) override;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_TEST_SHARE_KIT_SERVICE_H_
