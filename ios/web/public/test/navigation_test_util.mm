// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/navigation_test_util.h"

#import "base/test/ios/wait_util.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace test {

void LoadUrl(web::WebState* web_state, const GURL& url) {
  NavigationManager* navigation_manager = web_state->GetNavigationManager();
  NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  navigation_manager->LoadURLWithParams(params);
}

bool WaitForPageToFinishLoading(WebState* web_state) {
  return WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return !web_state->IsLoading();
  });
}

}  // namespace test
}  // namespace web
