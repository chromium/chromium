// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon_configurator.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/shared/model/url/url_util.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_fetch_info.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_group_item_utils.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_snapshot_and_favicon.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/web_state_tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/web/public/web_state.h"
#import "ui/gfx/favicon_size.h"

namespace {

// Size for the default favicon.
const CGFloat kFaviconSize = 16.0;
// The minimum size of tab favicons.
constexpr CGFloat kFaviconMinimumSize = 8.0;

// Returns a key for the given `group_item`.
NSValue* TabGroupIdentifierKeyforTabGroupItem(TabGroupItem* group_item) {
  return [NSValue valueWithPointer:group_item.tabGroupIdentifier];
}

// Wrapper that is used to convert `completion` so that it can be used
// as a callback for FaviconLoader.
void OnFaviconFetched(TabSnapshotAndFavicon* tab_snapshot_and_favicon,
                      UIImageConfiguration* configuration,
                      TabSnapshotAndFaviconConfigurator::Completion completion,
                      FaviconAttributes* attributes,
                      bool cached) {
  // Nothing to do if the favicon returned is the default favicon.
  if (cached && !attributes.faviconImage) {
    return;
  }

  tab_snapshot_and_favicon.favicon =
      attributes.faviconImage
          ?: DefaultSymbolWithConfiguration(kGlobeAmericasSymbol,
                                            configuration);

  completion(tab_snapshot_and_favicon);
}

// Wraps `block` into another block that ignores the first `IgnoredArgs...`.
template <typename... IgnoredArgs, typename... Args>
auto IgnoreArgs(void (^block)(Args...)) -> void (^)(IgnoredArgs..., Args...) {
  return ^(IgnoredArgs..., Args... args) {
    block(args...);
  };
}

}  // namespace

class TabSnapshotAndFaviconConfigurator::RequestInfo {
 public:
  RequestInfo(web::WebState* web_state) : web_state_(web_state) {}

  RequestInfo(const RequestInfo&) = delete;
  RequestInfo& operator=(const RequestInfo&) = delete;

  virtual ~RequestInfo() = default;

  // Returns the WebState to use for this request.
  web::WebState* web_state() const { return web_state_; }

  // Returns whether the snapshot should be fetched or not.
  virtual bool ShouldFetchSnapshot() const = 0;

  // Returns the favicon to use for NTP (can use `configuration` if
  // the favicon uses a symbol).
  virtual UIImage* GetNTPFavicon(UIImageConfiguration* configuration) const = 0;

 private:
  const raw_ptr<web::WebState> web_state_;
};

class TabSnapshotAndFaviconConfigurator::TabGroupItemRequestInfo
    : public TabSnapshotAndFaviconConfigurator::RequestInfo {
 public:
  explicit TabGroupItemRequestInfo(web::WebState* web_state,
                                   TabGroupItem* group_item,
                                   NSInteger request_index)
      : RequestInfo(web_state),
        group_item_(group_item),
        request_index_(request_index) {}

  bool ShouldFetchSnapshot() const override {
    const TabGroup* tab_group = group_item_.tabGroup;
    if (!tab_group) {
      return true;
    }

    return ShouldFetchSnapshotForTabInGroup(request_index_,
                                            tab_group->range().count());
  }

  UIImage* GetNTPFavicon(UIImageConfiguration* configuration) const override {
    return CustomSymbolWithConfiguration(kChromeProductSymbol, configuration);
  }

 private:
  TabGroupItem* const group_item_;
  const NSInteger request_index_;
};

class TabSnapshotAndFaviconConfigurator::TabSwitcherItemRequestInfo
    : public TabSnapshotAndFaviconConfigurator::RequestInfo {
 public:
  TabSwitcherItemRequestInfo(WebStateTabSwitcherItem* tab_item,
                             bool fetch_snapshot)
      : RequestInfo(tab_item.webState),
        tab_item_(tab_item),
        fetch_snapshot_(fetch_snapshot) {}

  bool ShouldFetchSnapshot() const override { return fetch_snapshot_; }

  UIImage* GetNTPFavicon(UIImageConfiguration* configuration) const override {
    return tab_item_.NTPFavicon;
  }

 private:
  WebStateTabSwitcherItem* const tab_item_;
  const bool fetch_snapshot_;
};

TabSnapshotAndFaviconConfigurator::TabSnapshotAndFaviconConfigurator(
    FaviconLoader* favicon_loader,
    SnapshotBrowserAgent* snapshot_browser_agent)
    : favicon_loader_(favicon_loader),
      snapshot_browser_agent_(snapshot_browser_agent) {
  group_item_fetches_ = [[NSMutableDictionary alloc] init];
}

TabSnapshotAndFaviconConfigurator::~TabSnapshotAndFaviconConfigurator() {}

void TabSnapshotAndFaviconConfigurator::FetchSnapshotAndFaviconForTabGroupItem(
    TabGroupItem* group_item,
    WebStateList* web_state_list,
    CompletionWithTabGroupItem completion) {
  const TabGroup* tab_group = group_item.tabGroup;
  if (!tab_group || !web_state_list) {
    return;
  }
  NSInteger tab_count = tab_group->range().count();
  NSInteger number_of_requests = TabGroupItemDisplayedVisualCount(tab_count);

  // Overwrite any previous request for this item.
  NSUUID* request_id = [NSUUID UUID];
  TabGroupItemFetchInfo* fetch_info = [[TabGroupItemFetchInfo alloc]
      initWithRequestID:request_id
      initialFetchCount:TabGroupItemFetchRequestsCount(tab_count)];
  group_item_fetches_[TabGroupIdentifierKeyforTabGroupItem(group_item)] =
      fetch_info;

  NSInteger first_index = tab_group->range().range_begin();
  for (NSInteger request_index = 0; request_index < number_of_requests;
       request_index++) {
    web::WebState* web_state =
        web_state_list->GetWebStateAt(first_index + request_index);
    CHECK(web_state);
    FetchSnapshotAndFaviconFromWebState(group_item, web_state, request_index,
                                        request_id, completion);
  }
}

void TabSnapshotAndFaviconConfigurator::
    FetchSingleSnapshotAndFaviconFromWebState(web::WebState* web_state,
                                              Completion completion) {
  FetchSnapshotAndFaviconFromWebState(
      /*group_item=*/nil, web_state,
      /*request_index=*/0, /*request_id=*/nil,
      IgnoreArgs<TabGroupItem*, NSInteger>(completion));
}

void TabSnapshotAndFaviconConfigurator::
    FetchSnapshotAndFaviconForTabSwitcherItem(
        WebStateTabSwitcherItem* tab_item,
        CompletionWithTabSwitcherItem completion) {
  FetchSnapshotAndFaviconForTabSwitcherItem(tab_item, /*fetch_snapshot=*/true,
                                            completion);
}

void TabSnapshotAndFaviconConfigurator::FetchFaviconForTabSwitcherItem(
    WebStateTabSwitcherItem* tab_item,
    CompletionWithTabSwitcherItem completion) {
  FetchSnapshotAndFaviconForTabSwitcherItem(tab_item, /*fetch_snapshot=*/false,
                                            completion);
}

#pragma mark - Private

void TabSnapshotAndFaviconConfigurator::FetchSnapshotAndFaviconFromWebState(
    TabGroupItem* group_item,
    web::WebState* web_state,
    NSInteger request_index,
    NSUUID* request_id,
    CompletionWithTabGroupItem completion) {
  FetchSnapshotAndFaviconInternal(
      TabGroupItemRequestInfo(web_state, group_item, request_index),
      base::CallbackToBlock(
          base::BindRepeating(&TabSnapshotAndFaviconConfigurator::
                                  OnSnapshotAndFaviconFromWebStateFetched,
                              weak_factory_.GetWeakPtr(), group_item,
                              request_index, request_id, completion)));
}

void TabSnapshotAndFaviconConfigurator::
    FetchSnapshotAndFaviconForTabSwitcherItem(
        WebStateTabSwitcherItem* tab_item,
        bool fetch_snapshot,
        CompletionWithTabSwitcherItem completion) {
  if (!tab_item.webState) {
    completion(tab_item, nil);
    return;
  }

  FetchSnapshotAndFaviconInternal(
      TabSwitcherItemRequestInfo(tab_item, fetch_snapshot),
      base::CallbackToBlock(base::BindRepeating(completion, tab_item)));
}

void TabSnapshotAndFaviconConfigurator::FetchSnapshotAndFaviconInternal(
    const RequestInfo& request_info,
    Completion completion) {
  web::WebState* web_state = request_info.web_state();
  CHECK(web_state);

  TabSnapshotAndFavicon* tab_snapshot_and_favicon =
      [[TabSnapshotAndFavicon alloc] init];

  if (request_info.ShouldFetchSnapshot() && snapshot_browser_agent_) {
    snapshot_browser_agent_->RetrieveSnapshotWithID(
        SnapshotID(web_state->GetUniqueIdentifier()), SnapshotKindColor,
        ^(UIImage* snapshot) {
          tab_snapshot_and_favicon.snapshot = snapshot;
          completion(tab_snapshot_and_favicon);
        });
  }

  UIImageConfiguration* configuration = [UIImageSymbolConfiguration
      configurationWithPointSize:kFaviconSize
                          weight:UIImageSymbolWeightBold
                           scale:UIImageSymbolScaleMedium];

  // NTP favicon does not need to be fetched.
  const GURL& url = web_state->GetVisibleURL();
  if (IsUrlNtp(url)) {
    tab_snapshot_and_favicon.favicon =
        request_info.GetNTPFavicon(configuration);
    completion(tab_snapshot_and_favicon);
    return;
  }

  // Check whether the favicon is cached by the favicon driver.
  if (favicon::FaviconDriver* favicon_driver =
          favicon::WebFaviconDriver::FromWebState(web_state)) {
    gfx::Image favicon = favicon_driver->GetFavicon();
    if (!favicon.IsEmpty()) {
      tab_snapshot_and_favicon.favicon = favicon.ToUIImage();
      completion(tab_snapshot_and_favicon);
      return;
    }
  }

  // If the favicon loader is not available, use a default favicon.
  if (!favicon_loader_) {
    tab_snapshot_and_favicon.favicon =
        DefaultSymbolWithConfiguration(kGlobeAmericasSymbol, configuration);
    completion(tab_snapshot_and_favicon);
    return;
  }

  // Fetch the favicon asynchronously.
  const bool fallback_to_google_server = true;
  favicon_loader_->FaviconForPageUrl(
      url, kFaviconSize, kFaviconMinimumSize, fallback_to_google_server,
      base::CallbackToBlock(base::BindRepeating(&OnFaviconFetched,
                                                tab_snapshot_and_favicon,
                                                configuration, completion)));
}

void TabSnapshotAndFaviconConfigurator::OnSnapshotAndFaviconFromWebStateFetched(
    TabGroupItem* group_item,
    NSInteger request_index,
    NSUUID* request_id,
    CompletionWithTabGroupItem completion,
    TabSnapshotAndFavicon* tab_snapshot_and_favicon) {
  if (!group_item) {
    completion(nil, request_index, tab_snapshot_and_favicon);
    return;
  }

  NSValue* group_key = TabGroupIdentifierKeyforTabGroupItem(group_item);

  // Check if the fetch is still the active one before proceeding.
  TabGroupItemFetchInfo* current_fetch_info = group_item_fetches_[group_key];
  if (![current_fetch_info.requestID isEqual:request_id]) {
    return;
  }

  [current_fetch_info decrementRemainingFetches];

  // If all fetches are completed, remove the `current_fetch_info` from
  // dictionary.
  if ([current_fetch_info currentRemainingFetches] == 0) {
    [group_item_fetches_ removeObjectForKey:group_key];
  }

  // Check that the group still exists.
  if (!group_item.tabGroup) {
    return;
  }

  completion(group_item, request_index, tab_snapshot_and_favicon);
}
