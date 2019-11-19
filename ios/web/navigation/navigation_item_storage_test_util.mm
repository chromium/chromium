// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_item_storage_test_util.h"

#import "ios/web/public/session/crw_navigation_item_storage.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

BOOL ItemStoragesAreEqual(CRWNavigationItemStorage* item1,
                          CRWNavigationItemStorage* item2) {
  return item1.virtualURL == item2.virtualURL &&
         item1.referrer.url == item2.referrer.url &&
         item1.referrer.policy == item2.referrer.policy &&
         item1.timestamp == item2.timestamp && item1.title == item2.title &&
         item1.displayState == item2.displayState &&
         item1.shouldSkipRepostFormConfirmation ==
             item2.shouldSkipRepostFormConfirmation &&
         item1.userAgentType == item2.userAgentType &&
         [item1.POSTData isEqualToData:item2.POSTData] &&
         [item1.HTTPRequestHeaders
             isEqualToDictionary:item2.HTTPRequestHeaders];
}

}  // namespace web
