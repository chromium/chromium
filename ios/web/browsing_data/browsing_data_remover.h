// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#import <Foundation/Foundation.h>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#import "ios/web/public/browsing_data/browsing_data_removing_util.h"

@protocol BrowsingDataRemoverObserver;
@class WKWebView;

namespace web {

class BrowserState;

// Class used to removes the browsing data stored by the WebKit data store.
class BrowsingDataRemover : public base::SupportsUserData::Data {
 public:
  explicit BrowsingDataRemover(web::BrowserState* browser_state);
  ~BrowsingDataRemover() override;

  // Returns the BrowsingDataRemover associated with |browser_state.|
  // Lazily creates one if an BrowsingDataRemover is not already associated with
  // the |browser_state|. |browser_state| cannot be a nullptr.
  static BrowsingDataRemover* FromBrowserState(BrowserState* browser_state);

  // Clears the browsing data. |modified_since| is the data since which all data
  // is removed. |closure| is called when the browsing data have been cleared.
  void ClearBrowsingData(ClearBrowsingDataMask types,
                         base::Time modified_since,
                         base::OnceClosure closure);

  void AddObserver(id<BrowsingDataRemoverObserver> observer);
  void RemoveObserver(id<BrowsingDataRemoverObserver> observer);

 private:
  web::BrowserState* browser_state_;  // weak, owns this object.
  // The list of observers. Holds weak references.
  NSHashTable<id<BrowsingDataRemoverObserver>>* observers_list_;

  // Dummy WKWebView. A WKWebView object is created before deleting cookies. and
  // is deleted after deleting cookies is completed. this is a workaround that
  // makes sure that there is a WKWebView object alive while accessing
  // WKHTTPCookieStore.
  WKWebView* dummy_web_view_ = nil;

  base::WeakPtrFactory<BrowsingDataRemover> weak_ptr_factory_;
};

}  // namespace web

#endif  // IOS_WEB_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
