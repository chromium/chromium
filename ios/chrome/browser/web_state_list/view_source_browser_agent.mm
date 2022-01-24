// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/view_source_browser_agent.h"

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/web_state_list/tab_insertion_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(ViewSourceBrowserAgent)

ViewSourceBrowserAgent::ViewSourceBrowserAgent(Browser* browser)
    : browser_(browser) {}

ViewSourceBrowserAgent::~ViewSourceBrowserAgent() {}

void ViewSourceBrowserAgent::ViewSourceForActiveWebState() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  DCHECK(web_state);

  NSString* script = @"document.documentElement.outerHTML;";
  auto completionHandlerBlock = ^(id result, NSError*) {
    if (!web_state)
      return;
    if (![result isKindOfClass:[NSString class]])
      result = @"Not an HTML page";
    InsertSourceViewTab(result, web_state);
  };
  [web_state->GetJSInjectionReceiver()
      executeJavaScript:script
      completionHandler:completionHandlerBlock];
}

void ViewSourceBrowserAgent::InsertSourceViewTab(NSString* source,
                                                 web::WebState* web_state) {
  std::string base64HTML;
  base::Base64Encode(base::SysNSStringToUTF8(source), &base64HTML);
  GURL URL(std::string("data:text/plain;charset=utf-8;base64,") + base64HTML);
  web::Referrer referrer(web_state->GetLastCommittedURL(),
                         web::ReferrerPolicyDefault);
  web::NavigationManager::WebLoadParams loadParams(URL);
  loadParams.referrer = referrer;
  loadParams.transition_type = ui::PAGE_TRANSITION_LINK;
  TabInsertionBrowserAgent* insertionAgent =
      TabInsertionBrowserAgent::FromBrowser(browser_);
  insertionAgent->InsertWebState(
      loadParams, web_state, true, TabInsertion::kPositionAutomatically,
      /*in_background=*/false, /*inherit_opener=*/false);
}
