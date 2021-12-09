// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/text_fragments/text_fragments_mediator.h"

#import <memory>

#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TextFragmentsMediator

- (void)userTappedTextFragmentInWebState:(web::WebState*)webState {
  // TODO(crbug.com/1259227): Right now, this treats every tap as if the user
  // wants to remove the highlight. Instead, the user should be provided with a
  // menu to select an action to take.
  web::TextFragmentsManager::FromWebState(webState)->RemoveHighlights();
}

- (void)registerWithWebState:(web::WebState*)webState {
  DCHECK(web::TextFragmentsManager::FromWebState(webState));
  // When a new WebState is available, get the manager and attach ourselves. The
  // manager holds a weak reference and has a default behavior if no delegate is
  // available, so there's no need to explicitly detach ourselves on
  // destruction.
  web::TextFragmentsManager::FromWebState(webState)->RegisterDelegate(self);
}

@end
