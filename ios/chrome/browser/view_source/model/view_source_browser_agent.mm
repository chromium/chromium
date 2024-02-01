// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/view_source/model/view_source_browser_agent.h"

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/tab_insertion/model/tab_insertion_browser_agent.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"

BROWSER_USER_DATA_KEY_IMPL(ViewSourceBrowserAgent)

ViewSourceBrowserAgent::ViewSourceBrowserAgent(Browser* browser)
    : browser_(browser) {}

ViewSourceBrowserAgent::~ViewSourceBrowserAgent() {}

void ViewSourceBrowserAgent::ViewSourceForActiveWebState() {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  DCHECK(web_state);

  web::WebFrame* web_frame =
      web_state->GetPageWorldWebFramesManager()->GetMainWebFrame();
  static const char16_t kScript[] = u"document.documentElement.outerHTML;";

  web_frame->ExecuteJavaScript(
      kScript,
      base::BindOnce(
          &ViewSourceBrowserAgent::OnHandleViewSourceForActiveWebStateResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void ViewSourceBrowserAgent::OnHandleViewSourceForActiveWebStateResult(
    const base::Value* value) {
  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  DCHECK(web_state);

  NSString* result;
  if (value->is_string()) {
    result = base::SysUTF8ToNSString(value->GetString());
  } else {
    result = @"Not an HTML page";
  }
  InsertSourceViewTab(result, web_state);
}

void ViewSourceBrowserAgent::InsertSourceViewTab(NSString* source,
                                                 web::WebState* web_state) {
  std::string base64HTML = base::Base64Encode(base::SysNSStringToUTF8(source));
  GURL URL(std::string("data:text/plain;charset=utf-8;base64,") + base64HTML);
  web::Referrer referrer(web_state->GetLastCommittedURL(),
                         web::ReferrerPolicyDefault);
  web::NavigationManager::WebLoadParams loadParams(URL);
  loadParams.referrer = referrer;
  loadParams.transition_type = ui::PAGE_TRANSITION_LINK;
  TabInsertionBrowserAgent* insertionAgent =
      TabInsertionBrowserAgent::FromBrowser(browser_);
  TabInsertion::Params insertionParams;
  insertionParams.opened_by_dom = true;
  insertionAgent->InsertWebState(loadParams, insertionParams);
}
