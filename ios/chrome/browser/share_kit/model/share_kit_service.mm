// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

ShareKitService::ShareKitService() = default;

ShareKitService::~ShareKitService() = default;

void ShareKitService::ShareGroup(const TabGroup* group,
                                 UIViewController* base_view_controller) {}

void ShareKitService::ShareGroup(const TabGroup* group,
                                 UIViewController* base_view_controller,
                                 id<ApplicationCommands> commandsHandler) {
  ShareGroup(group, base_view_controller);
}
