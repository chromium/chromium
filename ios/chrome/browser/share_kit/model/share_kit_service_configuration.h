// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_CONFIGURATION_H_
#define IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_CONFIGURATION_H_

#import "base/memory/raw_ptr.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing {
class DataSharingService;
}  // namespace data_sharing

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

class AuthenticationService;
class TabGroupFaviconsGridConfigurator;

// Configuration object used by the ShareKitService.
struct ShareKitServiceConfiguration {
  ShareKitServiceConfiguration(
      raw_ptr<signin::IdentityManager> identity_manager,
      raw_ptr<AuthenticationService> authentication_service,
      raw_ptr<data_sharing::DataSharingService> data_sharing_service,
      raw_ptr<tab_groups::TabGroupSyncService> sync_service,
      std::unique_ptr<TabGroupFaviconsGridConfigurator>
          favicons_grid_configurator);
  ShareKitServiceConfiguration(const ShareKitServiceConfiguration&) = delete;
  ShareKitServiceConfiguration& operator=(const ShareKitServiceConfiguration&) =
      delete;
  ~ShareKitServiceConfiguration();

  // IdentityManager to observe changes of primary account.
  raw_ptr<signin::IdentityManager> identity_manager;

  // The authentication service to get the primary account.
  raw_ptr<AuthenticationService> authentication_service;

  // The data sharing service to handle link creation.
  raw_ptr<data_sharing::DataSharingService> data_sharing_service;

  // The service to handle tab group sync.
  raw_ptr<tab_groups::TabGroupSyncService> sync_service;

  // Configures favicons for TabGroupFaviconsGrid objects.
  std::unique_ptr<TabGroupFaviconsGridConfigurator> favicons_grid_configurator;
};

#endif  // IOS_CHROME_BROWSER_SHARE_KIT_MODEL_SHARE_KIT_SERVICE_CONFIGURATION_H_
