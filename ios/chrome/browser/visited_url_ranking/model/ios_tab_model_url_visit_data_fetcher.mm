// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/visited_url_ranking/model/ios_tab_model_url_visit_data_fetcher.h"

#import "components/sync_device_info/device_info.h"
#import "components/url_deduplication/url_deduplication_helper.h"
#import "components/visited_url_ranking/public/fetcher_config.h"
#import "components/visited_url_ranking/public/url_visit.h"
#import "components/visited_url_ranking/public/url_visit_util.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_session_tab_helper.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"

namespace visited_url_ranking {

namespace {

base::Time GetWebStateLastModificationTime(web::WebState* web_state) {
  base::Time last_modification = web_state->GetLastActiveTime();
  if (web_state->IsRealized()) {
    web::NavigationItem* item =
        web_state->GetNavigationManager()->GetLastCommittedItem();
    if (item) {
      last_modification = item->GetTimestamp();
    }
  }
  return last_modification;
}

URLVisitAggregate::Tab MakeAggregateTabFromWebState(
    web::WebState* web_state,
    base::Time last_modification) {
  syncer::DeviceInfo::FormFactor form_factor =
      ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET
          ? syncer::DeviceInfo::FormFactor::kTablet
          : syncer::DeviceInfo::FormFactor::kPhone;
  const GURL& url = web_state->GetLastCommittedURL();
  visited_url_ranking::URLVisit visit(
      url, web_state->GetTitle(), last_modification, form_factor,
      visited_url_ranking::URLVisit::Source::kLocal);
  int32_t tab_id =
      IOSChromeSessionTabHelper::FromWebState(web_state)->session_id().id();

  URLVisitAggregate::Tab tab(tab_id, visit);
  return tab;
}

}  // namespace

IOSTabModelURLVisitDataFetcher::IOSTabModelURLVisitDataFetcher(
    ProfileIOS* profile)
    : profile_(profile) {}

IOSTabModelURLVisitDataFetcher::~IOSTabModelURLVisitDataFetcher() {}

void IOSTabModelURLVisitDataFetcher::FetchURLVisitData(
    const FetchOptions& options,
    const FetcherConfig& config,
    FetchResultCallback callback) {
  // OTR URL should never be processed.
  CHECK(!profile_->IsOffTheRecord());

  std::map<URLMergeKey, URLVisitAggregate::TabData> url_visit_tab_data_map;
  const BrowserList* browser_list = BrowserListFactory::GetForProfile(profile_);
  for (Browser* browser : browser_list->BrowsersOfType(
           BrowserList::BrowserType::kRegularAndInactive)) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int i = 0; i < web_state_list->count(); i++) {
      web::WebState* web_state = web_state_list->GetWebStateAt(i);
      const GURL& url = web_state->GetLastCommittedURL();
      if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
        continue;
      }
      base::Time last_modification = GetWebStateLastModificationTime(web_state);
      if (last_modification < options.begin_time) {
        continue;
      }

      auto url_key = ComputeURLMergeKey(url, web_state->GetTitle(),
                                        config.deduplication_helper);
      auto it = url_visit_tab_data_map.find(url_key);
      bool tab_data_map_already_has_url_entry =
          (it != url_visit_tab_data_map.end());
      if (!tab_data_map_already_has_url_entry) {
        url_visit_tab_data_map.emplace(
            url_key,
            MakeAggregateTabFromWebState(web_state, last_modification));
      }

      auto& tab_data = url_visit_tab_data_map.at(url_key);
      if (tab_data_map_already_has_url_entry) {
        if (tab_data.last_active_tab.visit.last_modified < last_modification) {
          tab_data.last_active_tab =
              MakeAggregateTabFromWebState(web_state, last_modification);
        }
        tab_data.tab_count += 1;
      }

      tab_data.last_active =
          std::max(tab_data.last_active, web_state->GetLastActiveTime());
      tab_data.pinned =
          tab_data.pinned || web_state_list->IsWebStatePinnedAt(i);
      tab_data.in_group =
          tab_data.in_group || web_state_list->GetGroupOfWebStateAt(i);
    }
  }
  std::map<URLMergeKey, URLVisitAggregate::URLVisitVariant>
      url_visit_variant_map;
  std::transform(
      std::make_move_iterator(url_visit_tab_data_map.begin()),
      std::make_move_iterator(url_visit_tab_data_map.end()),
      std::inserter(url_visit_variant_map, url_visit_variant_map.end()),
      [](auto kv) { return std::make_pair(kv.first, std::move(kv.second)); });
  std::move(callback).Run(
      {FetchResult::Status::kSuccess, std::move(url_visit_variant_map)});
}

}  // namespace visited_url_ranking
