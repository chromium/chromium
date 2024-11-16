// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

ShareKitService::ShareKitService() = default;

ShareKitService::~ShareKitService() = default;

void ShareKitService::PrimaryAccountChanged() {}

void ShareKitService::ReadGroups(ShareKitReadConfiguration* config) {}

id<ShareKitAvatarPrimitive> ShareKitService::AvatarImage(
    ShareKitAvatarConfiguration* config) {
  return nil;
}

void ShareKitService::LeaveGroup(ShareKitLeaveConfiguration* config) {}

void ShareKitService::DeleteGroup(ShareKitDeleteConfiguration* config) {}

void ShareKitService::LookupGaiaIdByEmail(
    ShareKitLookupGaiaIDConfiguration* config) {}
