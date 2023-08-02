// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/find_in_page/find_in_page_manager.h"
#import "ios/web/find_in_page/find_in_page_manager_impl.h"

namespace web {

void FindInPageManager::CreateForWebState(WebState* web_state) {
  DCHECK(web_state);
  // Should not create this if the web state is not realized.
  DCHECK(web_state->IsRealized());
  if (!FromWebState(web_state)) {
    web_state->SetUserData(UserDataKey(),
                           std::make_unique<FindInPageManagerImpl>(web_state));
  }
}

}  // namespace web
