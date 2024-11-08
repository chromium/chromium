// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service_configuration.h"
#import "ios/public/provider/chrome/browser/share_kit/share_kit_api.h"

namespace {

// Chromium implementation of the ShareKitService. Does nothing.
class ChromiumShareKitService final : public ShareKitService {
 public:
  ChromiumShareKitService() = default;
  ~ChromiumShareKitService() final = default;

  // ShareKitService.
  bool IsSupported() const override { return false; }
  void ShareGroup(ShareKitShareGroupConfiguration* config) override {}
  void ManageGroup(ShareKitManageConfiguration* config) override {}
  void JoinGroup(ShareKitJoinConfiguration* config) override {}
  UIViewController* FacePile(ShareKitFacePileConfiguration* config) override {
    return nil;
  }
  void ReadGroups(ShareKitReadConfiguration* config) override {}
  id<ShareKitAvatarPrimitive> AvatarImage(
      ShareKitAvatarConfiguration* config) override {
    return nil;
  }
};

}  // namespace

namespace ios::provider {

std::unique_ptr<ShareKitService> CreateShareKitService(
    std::unique_ptr<ShareKitServiceConfiguration> configuration) {
  return std::make_unique<ChromiumShareKitService>();
}

}  // namespace ios::provider
