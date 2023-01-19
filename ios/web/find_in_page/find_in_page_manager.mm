// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/find_in_page_manager.h"
#import "ios/web/find_in_page/find_in_page_manager_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

FindInPageManager* FindInPageManager::GetOrCreateForWebState(
    WebState* web_state,
    bool use_find_interaction) {
  auto* manager = FindInPageManager::FromWebState(web_state);
  if (!manager) {
    FindInPageManagerImpl::CreateForWebState(web_state, use_find_interaction);
    manager = FindInPageManager::FromWebState(web_state);
    DCHECK(manager);
  }

  return manager;
}

}  // namespace web
