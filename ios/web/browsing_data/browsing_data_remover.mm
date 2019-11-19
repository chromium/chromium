// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/browsing_data/browsing_data_remover.h"

#import <WebKit/WebKit.h>

#import "base/ios/block_types.h"
#include "base/task/post_task.h"
#import "ios/web/browsing_data/browsing_data_remover_observer.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kWebBrowsingDataRemoverKeyName[] = "web_browsing_data_remover";
}  // namespace

namespace web {

BrowsingDataRemover::BrowsingDataRemover(web::BrowserState* browser_state)
    : browser_state_(browser_state), weak_ptr_factory_(this) {
  DCHECK(browser_state_);
  observers_list_ = [NSHashTable weakObjectsHashTable];
}

BrowsingDataRemover::~BrowsingDataRemover() {}

// static
BrowsingDataRemover* BrowsingDataRemover::FromBrowserState(
    BrowserState* browser_state) {
  DCHECK(browser_state);

  BrowsingDataRemover* browsing_data_remover =
      static_cast<BrowsingDataRemover*>(
          browser_state->GetUserData(kWebBrowsingDataRemoverKeyName));
  if (!browsing_data_remover) {
    browsing_data_remover = new BrowsingDataRemover(browser_state);
    browser_state->SetUserData(kWebBrowsingDataRemoverKeyName,
                               base::WrapUnique(browsing_data_remover));
  }
  return browsing_data_remover;
}

void BrowsingDataRemover::ClearBrowsingData(ClearBrowsingDataMask types,
                                            base::Time modified_since,
                                            base::OnceClosure closure) {
  __block base::OnceClosure block_closure = std::move(closure);

  // Converts browsing data types from ClearBrowsingDataMask to
  // WKWebsiteDataStore strings.
  NSMutableSet* data_types_to_remove = [[NSMutableSet alloc] init];

  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCacheStorage)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeDiskCache];
    [data_types_to_remove addObject:WKWebsiteDataTypeMemoryCache];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveAppCache)) {
    [data_types_to_remove
        addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveLocalStorage)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeSessionStorage];
    [data_types_to_remove addObject:WKWebsiteDataTypeLocalStorage];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveWebSQL)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeWebSQLDatabases];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveIndexedDB)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeIndexedDBDatabases];
  }
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCookies)) {
    [data_types_to_remove addObject:WKWebsiteDataTypeCookies];
  }

  if (![data_types_to_remove count]) {
    std::move(block_closure).Run();
    return;
  }

  for (id<BrowsingDataRemoverObserver> observer in observers_list_) {
    [observer willRemoveBrowsingData:this];
  }

  base::WeakPtr<BrowsingDataRemover> weak_ptr = weak_ptr_factory_.GetWeakPtr();
  ProceduralBlock completion_block = ^{
    if (BrowsingDataRemover* strong_ptr = weak_ptr.get()) {
      [strong_ptr->dummy_web_view_ removeFromSuperview];
      strong_ptr->dummy_web_view_ = nil;
      for (id<BrowsingDataRemoverObserver> observer in strong_ptr
               ->observers_list_) {
        [observer didRemoveBrowsingData:this];
      }
    }
    std::move(block_closure).Run();
  };

  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveVisitedLinks)) {
    ProceduralBlock previous_completion_block = completion_block;

    // TODO(crbug.com/557963): Purging the WKProcessPool is a workaround for
    // the fact that there is no public API to clear visited links in
    // WKWebView. Remove this workaround if/when that API is made public.
    // Note: Purging the WKProcessPool for clearing visisted links does have
    // the side-effect of also losing the in-memory cookies of WKWebView but
    // it is not a problem in practice since there is no UI to only have
    // visited links be removed but not cookies.
    completion_block = ^{
      if (BrowsingDataRemover* strong_ptr = weak_ptr.get()) {
        web::WKWebViewConfigurationProvider::FromBrowserState(
            strong_ptr->browser_state_)
            .Purge();
      }
      previous_completion_block();
    };
  }

  // TODO(crbug.com/661630): |dummy_web_view_| is created to allow
  // the -[WKWebsiteDataStore removeDataOfTypes:] API to access the cookiestore
  // and clear cookies. This is a workaround for
  // https://bugs.webkit.org/show_bug.cgi?id=149078. Remove this
  // workaround when it's not needed anymore.
  if (IsRemoveDataMaskSet(types, ClearBrowsingDataMask::kRemoveCookies)) {
    if (!dummy_web_view_) {
      dummy_web_view_ = [[WKWebView alloc] initWithFrame:CGRectZero];
      dummy_web_view_.hidden = YES;
      // It seems that the WKWebView needs to be part of the view hierarchy to
      // prevent its out-of-process process from being suspended. If it is not
      // added to the view hierarchy, tests are failing when trying to
      // remove/check the cookies which indicates that we might have the same
      // issue with cookies. Adding the web view to the view hierarchy as a safe
      // guard.
      [[UIApplication sharedApplication].keyWindow insertSubview:dummy_web_view_
                                                         atIndex:0];
    }
  }

  NSDate* delete_begin_date =
      [NSDate dateWithTimeIntervalSince1970:modified_since.ToDoubleT()];
  [[WKWebsiteDataStore defaultDataStore] removeDataOfTypes:data_types_to_remove
                                             modifiedSince:delete_begin_date
                                         completionHandler:completion_block];
}

void BrowsingDataRemover::AddObserver(
    id<BrowsingDataRemoverObserver> observer) {
  [observers_list_ addObject:observer];
}

void BrowsingDataRemover::RemoveObserver(
    id<BrowsingDataRemoverObserver> observer) {
  [observers_list_ removeObject:observer];
}

}  // namespace web
