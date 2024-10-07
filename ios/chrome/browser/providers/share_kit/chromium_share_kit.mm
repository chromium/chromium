// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "ios/public/provider/chrome/browser/share_kit/share_kit_api.h"

namespace {

// Chromium implementation of the ShareKitService. Does nothing.
class ChromiumShareKitService final : public ShareKitService {
 public:
  ChromiumShareKitService() = default;
  ~ChromiumShareKitService() final = default;

  // ShareKitService.
  bool IsSupported() const override { return false; }
  void ShareGroup(const TabGroup* group,
                  UIViewController* base_view_controller) override {}
};

}  // namespace

namespace ios::provider {

std::unique_ptr<ShareKitService> CreateShareKitService(
    const ShareKitServiceConfiguration& configuration) {
  return std::make_unique<ChromiumShareKitService>();
}

}  // namespace ios::provider
