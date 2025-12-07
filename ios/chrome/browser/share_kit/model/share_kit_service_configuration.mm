// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service_configuration.h"

ShareKitServiceConfiguration::ShareKitServiceConfiguration(
    raw_ptr<signin::IdentityManager> identity_manager,
    raw_ptr<AuthenticationService> authentication_service,
    raw_ptr<data_sharing::DataSharingService> data_sharing_service,
    raw_ptr<collaboration::CollaborationService> collaboration_service,
    raw_ptr<tab_groups::TabGroupSyncService> sync_service,
    raw_ptr<TabGroupService> tab_group_service)
    : identity_manager(identity_manager),
      authentication_service(authentication_service),
      data_sharing_service(data_sharing_service),
      collaboration_service(collaboration_service),
      sync_service(sync_service),
      tab_group_service(tab_group_service) {}

ShareKitServiceConfiguration::~ShareKitServiceConfiguration() = default;
