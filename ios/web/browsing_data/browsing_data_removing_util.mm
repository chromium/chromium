// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/browsing_data/browsing_data_removing_util.h"

#import <WebKit/WebKit.h>

#import "base/functional/callback_helpers.h"
#import "base/time/time.h"
#import "ios/web/common/uikit_ui_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/browser_state_utils.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

namespace web {
namespace {

// Converts ClearBrowsingDataMask to WKWebsiteDataStore strings.
NSSet<NSString*>* ConvertClearBrowsingDataMask(ClearBrowsingDataMask types) {
  NSMutableSet<NSString*>* result = [[NSMutableSet alloc] init];
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCacheStorage)) {
    [result addObject:WKWebsiteDataTypeDiskCache];
    [result addObject:WKWebsiteDataTypeMemoryCache];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveAppCache)) {
    [result addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveLocalStorage)) {
    [result addObject:WKWebsiteDataTypeSessionStorage];
    [result addObject:WKWebsiteDataTypeLocalStorage];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveWebSQL)) {
    [result addObject:WKWebsiteDataTypeWebSQLDatabases];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveIndexedDB)) {
    [result addObject:WKWebsiteDataTypeIndexedDBDatabases];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCookies)) {
    [result addObject:WKWebsiteDataTypeCookies];
  }
  return result;
}

}  // anonymous namespace

void ClearBrowsingData(BrowserState* browser_state,
                       ClearBrowsingDataMask types,
                       base::Time modified_since,
                       base::OnceClosure closure) {
  NSSet<NSString*>* types_to_remove = ConvertClearBrowsingDataMask(types);
  if (![types_to_remove count]) {
    std::move(closure).Run();
    return;
  }

  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveVisitedLinks)) {
    // TODO(crbug.com/41219991): Purging the WKProcessPool is a workaround for
    // the fact that there is no public API to clear visited links in
    // WKWebView. Remove this workaround if/when that API is made public.
    // Note: Purging the WKProcessPool for clearing visisted links does have
    // the side-effect of also losing the in-memory cookies of WKWebView but
    // it is not a problem in practice since there is no UI to only have
    // visited links be removed but not cookies.
    base::OnceClosure purge_process_pool = base::BindOnce(
        &WKWebViewConfigurationProvider::Purge,
        WKWebViewConfigurationProvider::FromBrowserState(browser_state)
            .AsWeakPtr());

    closure = std::move(purge_process_pool).Then(std::move(closure));
  }

  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCookies)) {
    // TODO(crbug.com/40491729): Create a dummy WKWebView to allow the APO
    // -[WKWebsiteDataStore removeDataOfType:] to access the cookie store
    // and clear cookies. This is a workaround that for the WebKit bug
    // https://bugs.webkit.org/show_bug.cgi?id=149078 and needs to be
    // removed when no longer necessary.
    WKWebView* dummy_web_view;
    dummy_web_view = [[WKWebView alloc] initWithFrame:CGRectZero];
    dummy_web_view.hidden = YES;

    // It seems that the WKWebView needs to be part of the view hierarchy to
    // prevent its out-of-process process from being suspended. If it is not
    // added to the view hierarchy, tests are failing when trying to
    // remove/check the cookies which indicates that we might have the same
    // issue with cookies. Adding the web view to the view hierarchy as a safe
    // guard.
    [GetAnyKeyWindow() insertSubview:dummy_web_view atIndex:0];

    base::OnceClosure remove_dummy_web_view = base::BindOnce(^{
      [dummy_web_view removeFromSuperview];
    });

    closure = std::move(remove_dummy_web_view).Then(std::move(closure));
  }

  WKWebsiteDataStore* data_store = GetDataStoreForBrowserState(browser_state);
  [data_store removeDataOfTypes:types_to_remove
                  modifiedSince:modified_since.ToNSDate()
              completionHandler:base::CallbackToBlock(std::move(closure))];
}

}  // namespace web
