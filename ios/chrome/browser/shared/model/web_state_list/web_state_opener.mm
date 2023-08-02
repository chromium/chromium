// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"

#import "base/check.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

namespace {
int NavigationIndexFromWebState(web::WebState* web_state) {
  if (!web_state) {
    return -1;
  }

  DCHECK(web_state->GetNavigationManager());
  return web_state->GetNavigationManager()->GetLastCommittedItemIndex();
}
}  // namespace

WebStateOpener::WebStateOpener() : WebStateOpener(nullptr) {}

WebStateOpener::WebStateOpener(web::WebState* opener)
    : WebStateOpener(opener, NavigationIndexFromWebState(opener)) {}

WebStateOpener::WebStateOpener(web::WebState* opener, int navigation_index)
    : opener(opener), navigation_index(navigation_index) {}
