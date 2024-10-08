// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <memory>

#import "ios/chrome/browser/share_kit/model/test_share_kit_service.h"
#import "ios/public/provider/chrome/browser/share_kit/share_kit_api.h"

namespace ios::provider {

std::unique_ptr<ShareKitService> CreateShareKitService(
    const ShareKitServiceConfiguration& configuration) {
  return std::make_unique<TestShareKitService>();
}

}  // namespace ios::provider
