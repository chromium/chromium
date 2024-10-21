// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service.h"

#import "ios/chrome/browser/share_kit/model/share_kit_face_pile_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_share_group_configuration.h"

ShareKitService::ShareKitService() = default;

ShareKitService::~ShareKitService() = default;

void ShareKitService::ShareGroup(ShareKitShareGroupConfiguration* config) {
  ShareGroup(config.tabGroup, config.baseViewController,
             config.applicationHandler);
}

void ShareKitService::ShareGroup(const TabGroup* group,
                                 UIViewController* base_view_controller,
                                 id<ApplicationCommands> commandsHandler) {
}

UIViewController* ShareKitService::FacePile(NSString* collab_id) {
  return nil;
}

UIViewController* ShareKitService::FacePile(
    ShareKitFacePileConfiguration* config) {
  return FacePile(config.collabID);
}
