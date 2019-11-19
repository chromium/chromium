// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/find_in_page/find_in_page_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

const char kFindInPageSearch[] = "findInPage.findString";

const char kFindInPagePump[] = "findInPage.pumpSearch";

const char kFindInPageSelectAndScrollToMatch[] =
    "findInPage.selectAndScrollToVisibleMatch";

const char kSelectAndScrollResultMatches[] = "matches";

const char kSelectAndScrollResultIndex[] = "index";

const char kSelectAndScrollResultContextString[] = "contextString";

const char kFindInPageStop[] = "findInPage.stop";

}  // namespace web
