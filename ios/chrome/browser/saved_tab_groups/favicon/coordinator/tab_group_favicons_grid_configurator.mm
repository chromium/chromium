// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"

#import "base/notreached.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ui/gfx/favicon_size.h"

namespace {

// Size for the default favicon.
constexpr CGFloat kFaviconSize = 16;

// Updates the `favicons_grid`s favicon with `favicon` at `index`.
void UpdateFaviconsGrid(TabGroupFaviconsGrid* favicons_grid,
                        UIImage* favicon,
                        int index) {
  switch (index) {
    case 0:
      favicons_grid.favicon1 = favicon;
      break;
    case 1:
      favicons_grid.favicon2 = favicon;
      break;
    case 2:
      favicons_grid.favicon3 = favicon;
      break;
    case 3:
      favicons_grid.favicon4 = favicon;
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace

TabGroupFaviconsGridConfigurator::TabGroupFaviconsGridConfigurator(
    tab_groups::TabGroupSyncService* tab_group_sync_service,
    FaviconLoader* favicon_loader)
    : tab_group_sync_service_(tab_group_sync_service),
      favicon_loader_(favicon_loader) {}

TabGroupFaviconsGridConfigurator::~TabGroupFaviconsGridConfigurator() {}

void TabGroupFaviconsGridConfigurator::ConfigureFaviconsGrid(
    TabGroupFaviconsGrid* favicons_grid,
    const TabGroup* tab_group) {
  [favicons_grid resetFavicons];

  const auto saved_group =
      tab_group_sync_service_->GetGroup(tab_group->tab_group_id());
  if (!saved_group) {
    return;
  }
  ConfigureFaviconsGrid(favicons_grid, saved_group->saved_guid());
}

#pragma mark - Private

void TabGroupFaviconsGridConfigurator::ConfigureFaviconsGrid(
    TabGroupFaviconsGrid* favicons_grid,
    base::Uuid saved_group_id) {
  [favicons_grid resetFavicons];

  const auto saved_group = tab_group_sync_service_->GetGroup(saved_group_id);
  if (!saved_group) {
    return;
  }

  const auto saved_tabs = saved_group->saved_tabs();
  const int saved_tabs_count = saved_tabs.size();
  __weak TabGroupFaviconsGridConfigurationToken* weak_token =
      favicons_grid.configurationToken;
  UIImage* fallback_image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kFaviconSize),
      @[ [UIColor colorNamed:kGrey400Color] ]);

  // Display up to 4 favicons. If there are more than 4 saved tabs,
  // the last slot will display the total number of saved tabs.
  int end = saved_tabs_count > 4 ? 3 : saved_tabs_count;

  // Update the favicons.
  for (int index = 0; index < end; index++) {
    __block UIImage* favicon;
    favicon = fallback_image;
    const auto saved_tab = saved_tabs[index];
    favicon_loader_->FaviconForPageUrlOrHost(
        saved_tab.url(), gfx::kFaviconSize, ^(FaviconAttributes* attributes) {
          if (!weak_token) {
            return;
          }
          if (attributes.usesDefaultImage || !attributes.faviconImage) {
            UpdateFaviconsGrid(favicons_grid, favicon, index);
          } else {
            UpdateFaviconsGrid(favicons_grid, attributes.faviconImage, index);
          }
        });
  }
}
