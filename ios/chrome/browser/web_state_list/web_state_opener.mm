// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_opener.h"

#include "base/logging.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
int NavigationIndexFromWebState(web::WebState* web_state) {
  if (!web_state)
    return -1;

  DCHECK(web_state->GetNavigationManager());
  return web_state->GetNavigationManager()->GetLastCommittedItemIndex();
}
}  // namespace

WebStateOpener::WebStateOpener() : WebStateOpener(nullptr) {}

WebStateOpener::WebStateOpener(web::WebState* opener)
    : WebStateOpener(opener, NavigationIndexFromWebState(opener)) {}

WebStateOpener::WebStateOpener(web::WebState* opener, int navigation_index)
    : opener(opener), navigation_index(navigation_index) {}
