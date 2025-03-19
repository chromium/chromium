// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

#import "ios/chrome/browser/share_kit/model/share_kit_read_configuration.h"

ShareKitService::ShareKitService() = default;

ShareKitService::~ShareKitService() = default;

void ShareKitService::ReadGroups(ShareKitReadConfiguration* config) {}

void ShareKitService::ReadGroups(ShareKitReadGroupsConfiguration* config) {
  ShareKitReadConfiguration* inner_config =
      [[ShareKitReadConfiguration alloc] init];
  inner_config.groupsParam = config.groupsParam;
  inner_config.callback = std::move(config.callback);
  ReadGroups(inner_config);
}
