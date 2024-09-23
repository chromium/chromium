// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_mediator.h"

#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/bring_android_tabs/model/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/model/metrics.h"
#import "ios/chrome/browser/favicon/model/favicon_loader.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/synced_sessions/model/distant_tab.h"
#import "ios/chrome/browser/synced_sessions/model/synced_sessions_util.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/constants.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_consumer.h"
#import "ios/chrome/browser/bring_android_tabs/ui_bundled/tab_list_from_android_table_view_item.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"

namespace {

typedef NS_ENUM(NSInteger, ItemType) {
  TabItemType = kItemTypeEnumZero,
};

}  // namespace

@implementation TabListFromAndroidMediator {
  // Keyed service to retrieve active tabs from Android.
  raw_ptr<BringAndroidTabsToIOSService> _bringAndroidTabsService;
  // URL loader to open tabs when needed.
  raw_ptr<UrlLoadingBrowserAgent> _URLLoader;
  // Favicon loader.
  raw_ptr<FaviconLoader> _faviconLoader;
}

- (instancetype)
    initWithBringAndroidTabsService:(BringAndroidTabsToIOSService*)service
                          URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                      faviconLoader:(FaviconLoader*)faviconLoader {
  DCHECK(service != nil);
  if ((self = [super init])) {
    _bringAndroidTabsService = service;
    _URLLoader = URLLoader;
    _faviconLoader = faviconLoader;
  }
  return self;
}

- (void)setConsumer:(id<TabListFromAndroidConsumer>)consumer {
  _consumer = consumer;
  [_consumer setTabListItems:[self tableViewItemArray]];
}

#pragma mark - TableViewFaviconDataSource

- (void)faviconForPageURL:(CrURL*)URL
               completion:(void (^)(FaviconAttributes*))completion {
  _faviconLoader->FaviconForPageUrl(
      URL.gurl, kBringAndroidTabsFaviconSize, kMinFaviconSizePt,
      /*fallback_to_google_server=*/true, ^(FaviconAttributes* attributes) {
        completion(attributes);
      });
}

#pragma mark - TabListFromAndroidViewControllerDelegate

- (void)tabListFromAndroidViewControllerDidDismissWithSwipe:(BOOL)swiped
                                     numberOfDeselectedTabs:
                                         (int)countDeselected {
  bring_android_tabs::TabsListActionType action =
      swiped ? bring_android_tabs::TabsListActionType::kSwipeDown
             : bring_android_tabs::TabsListActionType::kCancel;
  base::UmaHistogramEnumeration(bring_android_tabs::kTabListActionHistogramName,
                                action);
  base::UmaHistogramCounts1000(
      bring_android_tabs::kDeselectedTabCountHistogramName, countDeselected);
  // The user journey to bring recent tabs on Android to iOS has finished.
  // Reload the service to update/clear the tabs.
  _bringAndroidTabsService->LoadTabs();
}

- (void)tabListFromAndroidViewControllerDidTapOpenButtonWithTabIndices:
    (NSArray<NSNumber*>*)tabIndices {
  base::UmaHistogramEnumeration(
      bring_android_tabs::kTabListActionHistogramName,
      bring_android_tabs::TabsListActionType::kOpenTabs);

  size_t numTabs = static_cast<size_t>([tabIndices count]);
  int deselected = static_cast<int>(
      _bringAndroidTabsService->GetNumberOfAndroidTabs() - numTabs);
  base::UmaHistogramCounts1000(
      bring_android_tabs::kDeselectedTabCountHistogramName, deselected);

  std::vector<size_t> indicesVector;
  for (size_t i = 0; i < numTabs; i++) {
    NSUInteger idx = [[tabIndices objectAtIndex:i] unsignedIntValue];
    indicesVector.push_back(static_cast<size_t>(idx));
  }
  _bringAndroidTabsService->OpenTabsAtIndices(indicesVector, _URLLoader);
}

#pragma mark - Private

// Returns an array of table view items corresponding to the user's tabs from
// Android.
- (NSArray<TabListFromAndroidTableViewItem*>*)tableViewItemArray {
  NSMutableArray<TabListFromAndroidTableViewItem*>* itemArray =
      [[NSMutableArray alloc] init];
  for (size_t idx = 0; idx < _bringAndroidTabsService->GetNumberOfAndroidTabs();
       idx++) {
    synced_sessions::DistantTab* distantTab =
        _bringAndroidTabsService->GetTabAtIndex(idx);
    TabListFromAndroidTableViewItem* tabListItem =
        [[TabListFromAndroidTableViewItem alloc] initWithType:TabItemType];
    tabListItem.title = base::SysUTF16ToNSString(distantTab->title);
    tabListItem.URL = [[CrURL alloc] initWithGURL:distantTab->virtual_url];
    [itemArray addObject:tabListItem];
  }
  return itemArray;
}

@end
