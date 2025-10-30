// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/favicon/coordinator/tab_group_favicons_grid_configurator.h"

#import "base/functional/callback_helpers.h"
#import "base/notreached.h"
#import "base/uuid.h"
#import "components/saved_tab_groups/public/tab_group_sync_service.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/saved_tab_groups/favicon/ui/tab_group_favicons_grid.h"
#import "ios/chrome/browser/share_kit/model/share_kit_preview_item.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ui/gfx/favicon_size.h"

namespace {

// The size in point of the favicons grid image.
constexpr CGFloat kFaviconsGridSize = 64.0;

// Size for the default favicon.
constexpr CGFloat kFaviconSize = 16;

// The minimum size of tab favicons.
constexpr CGFloat kFaviconMinimumSize = 8.0;

// Maximum delay to return favicons grid image.
constexpr base::TimeDelta kFetchFaviconsGridTimeDelay = base::Seconds(5);

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
        saved_tab.url(), gfx::kFaviconSize,
        ^(FaviconAttributes* attributes, bool cached) {
          if (!weak_token) {
            return;
          }
          if (!attributes.faviconImage) {
            UpdateFaviconsGrid(favicons_grid, favicon, index);
          } else {
            UpdateFaviconsGrid(favicons_grid, attributes.faviconImage, index);
          }
        });
  }
}

void TabGroupFaviconsGridConfigurator::ConfigureFaviconsGrid(
    TabGroupFaviconsGrid* favicons_grid,
    NSArray<ShareKitPreviewItem*>* preview_items) {
  [favicons_grid resetFavicons];

  const int tabs_count = (int)[preview_items count];

  // Display up to 4 favicons. If there are more than 4 tabs,
  // the last slot will display the total number of tabs.
  int end = tabs_count > 4 ? 3 : tabs_count;

  // Update the favicons.
  for (int index = 0; index < end; index++) {
    ShareKitPreviewItem* item = preview_items[index];
    UpdateFaviconsGrid(favicons_grid, item.image, index);
  }
}

void TabGroupFaviconsGridConfigurator::FetchFaviconsGrid(
    const TabGroup* tab_group,
    FaviconsGridImageCallBack callback) {
  const auto saved_group =
      tab_group_sync_service_->GetGroup(tab_group->tab_group_id());
  if (!saved_group) {
    std::move(callback).Run(nil);
    return;
  }

  const auto saved_tabs = saved_group->saved_tabs();
  const int saved_tabs_count = saved_tabs.size();

  // Display up to 4 favicons. If there are more than 4 tabs,
  // the last slot will display the total number of tabs.
  int end = saved_tabs_count > 4 ? 3 : saved_tabs_count;

  // Configure the favicons grid.
  CGRect frame = CGRectMake(0, 0, kFaviconsGridSize, kFaviconsGridSize);
  __block TabGroupFaviconsGrid* favicons_grid =
      [[TabGroupFaviconsGrid alloc] initWithFrame:frame];
  favicons_grid.translatesAutoresizingMaskIntoConstraints = NO;
  favicons_grid.numberOfTabs = saved_tabs_count;

  __block int completed_count = 0;
  __block auto completion_block = base::CallbackToBlock(std::move(callback));
  __block bool completion_block_executed = false;

  // If the favicon fetches take too long, the the partially fetched favicons
  // grid is returned.
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW,
                    kFetchFaviconsGridTimeDelay.InNanoseconds()),
      dispatch_get_main_queue(), ^{
        if (!completion_block_executed) {
          completion_block_executed = true;
          [favicons_grid layoutIfNeeded];
          completion_block(ImageFromView(favicons_grid, nil, UIEdgeInsetsZero));
        }
      });

  UIImage* fallback_image = SymbolWithPalette(
      DefaultSymbolWithPointSize(kGlobeAmericasSymbol, kFaviconSize),
      @[ [UIColor colorNamed:kGrey400Color] ]);

  // Update the favicons.
  for (int index = 0; index < end; index++) {
    __block UIImage* favicon = fallback_image;
    const auto saved_tab = saved_tabs[index];
    favicon_loader_->FaviconForPageUrl(
        saved_tab.url(), kFaviconSize, kFaviconMinimumSize,
        /*fallback_to_google_server=*/true,
        ^(FaviconAttributes* attributes, bool cached) {
          if (completion_block_executed) {
            return;
          }
          // Synchronously returned default favicon.
          if (cached && !attributes.faviconImage) {
            UpdateFaviconsGrid(favicons_grid, favicon, index);
            return;
          }
          if (attributes.faviconImage) {
            UpdateFaviconsGrid(favicons_grid, attributes.faviconImage, index);
          }
          completed_count++;

          // Check if all favicons have been fetched.
          if (completed_count == end) {
            completion_block_executed = true;
            [favicons_grid layoutIfNeeded];
            completion_block(
                ImageFromView(favicons_grid, nil, UIEdgeInsetsZero));
          }
        });
  }
}
