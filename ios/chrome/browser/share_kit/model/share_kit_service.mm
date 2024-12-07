// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

ShareKitService::ShareKitService() = default;

ShareKitService::~ShareKitService() = default;

void ShareKitService::CancelSession(NSString* session_id) {}

NSString* ShareKitService::ShareTabGroup(
    ShareKitShareGroupConfiguration* config) {
  return nil;
}

void ShareKitService::ShareGroup(ShareKitShareGroupConfiguration* config) {
  ShareTabGroup(config);
}

NSString* ShareKitService::ManageTabGroup(ShareKitManageConfiguration* config) {
  return nil;
}

void ShareKitService::ManageGroup(ShareKitManageConfiguration* config) {
  ManageTabGroup(config);
}

NSString* ShareKitService::JoinTabGroup(ShareKitJoinConfiguration* config) {
  return nil;
}

void ShareKitService::JoinGroup(ShareKitJoinConfiguration* config) {
  JoinTabGroup(config);
}
