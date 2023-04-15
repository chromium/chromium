// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"

#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/url/url_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation WebStateTabSwitcherItem

- (instancetype)initWithWebState:(web::WebState*)webState {
  DCHECK(webState);
  self = [super initWithIdentifier:webState->GetStableIdentifier()];
  if (self) {
    // chrome://newtab (NTP) tabs have no title.
    if (IsUrlNtp(webState->GetVisibleURL())) {
      self.hidesTitle = YES;
    }
    self.title = tab_util::GetTabTitle(webState);
    self.showsActivity = webState->IsLoading();
  }
  return self;
}

@end
