// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_FAVICON_COORDINATOR_TAB_GROUP_FAVICONS_GRID_CONFIGURATOR_H_
#define IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_FAVICON_COORDINATOR_TAB_GROUP_FAVICONS_GRID_CONFIGURATOR_H_

#import "base/memory/raw_ptr.h"

class FaviconLoader;
class TabGroup;
@class TabGroupFaviconsGrid;

namespace base {
class Uuid;
}  // namespace base

namespace tab_groups {
class TabGroupSyncService;
}  // namespace tab_groups

// Configures favicon for TabGroupFaviconsGrid objects.
class TabGroupFaviconsGridConfigurator {
 public:
  explicit TabGroupFaviconsGridConfigurator(
      tab_groups::TabGroupSyncService* tab_group_sync_service,
      FaviconLoader* favicon_loader);
  TabGroupFaviconsGridConfigurator(const TabGroupFaviconsGridConfigurator&) =
      delete;
  TabGroupFaviconsGridConfigurator& operator=(
      const TabGroupFaviconsGridConfigurator&) = delete;
  ~TabGroupFaviconsGridConfigurator();

  // Configures `favicons_grid` to display the favicons of the saved group with
  // ID `saved_group_id`.
  void ConfigureFaviconsGrid(TabGroupFaviconsGrid* favicons_grid,
                             base::Uuid saved_group_id);

  // Configures `favicons_grid` to display the favicons of `tab_group`.
  void ConfigureFaviconsGrid(TabGroupFaviconsGrid* favicons_grid,
                             const TabGroup* tab_group);

 private:
  // The tab group sync service to retrieve group info.
  raw_ptr<tab_groups::TabGroupSyncService> tab_group_sync_service_;
  // The object to retrieve tabs favicons.
  raw_ptr<FaviconLoader> favicon_loader_;
};

#endif  // IOS_CHROME_BROWSER_SAVED_TAB_GROUPS_FAVICON_COORDINATOR_TAB_GROUP_FAVICONS_GRID_CONFIGURATOR_H_
