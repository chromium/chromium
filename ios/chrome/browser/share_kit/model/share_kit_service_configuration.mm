// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/share_kit/model/share_kit_service_configuration.h"

#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"

ShareKitServiceConfiguration::ShareKitServiceConfiguration(
    raw_ptr<signin::IdentityManager> identity_manager,
    raw_ptr<AuthenticationService> authentication_service,
    raw_ptr<data_sharing::DataSharingService> data_sharing_service,
    raw_ptr<tab_groups::TabGroupSyncService> sync_service,
    std::unique_ptr<TabGroupFaviconsGridConfigurator>
        favicons_grid_configurator)
    : identity_manager(identity_manager),
      authentication_service(authentication_service),
      data_sharing_service(data_sharing_service),
      sync_service(sync_service),
      favicons_grid_configurator(std::move(favicons_grid_configurator)) {}

ShareKitServiceConfiguration::~ShareKitServiceConfiguration() = default;
