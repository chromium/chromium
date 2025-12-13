// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/url_loading_util.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/tab_restore_service_helper.h"
#import "ios/chrome/browser/prerender/model/prerender_browser_agent.h"
#import "ios/chrome/browser/sessions/model/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/model/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"
#import "ios/chrome/browser/web/model/web_navigation_util.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

bool IsURLAllowedInIncognito(const GURL& url) {
  // Most URLs are allowed in incognito; the following is an exception.
  return !(url.SchemeIs(kChromeUIScheme) &&
           url.GetHost() == kChromeUIHistoryHost);
}

void LoadJavaScriptURL(const GURL& url,
                       Browser* browser,
                       web::WebState* web_state) {
  DCHECK(url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(web_state);
  PrerenderBrowserAgent* prerender_browser_agent =
      PrerenderBrowserAgent::FromBrowser(browser);
  if (prerender_browser_agent) {
    prerender_browser_agent->CancelPrerender();
  }
  NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
      stringByRemovingPercentEncoding];
  if (web_state) {
    web_state->ExecuteUserJavaScript(jsToEval);
  }
}

void RestoreTab(const SessionID session_id,
                WindowOpenDisposition disposition,
                Browser* browser) {
  // iOS Chrome doesn't yet support restoring tabs to new windows.
  // TODO(crbug.com/40676931) : Support WINDOW restoration under multi-window.
  DCHECK(disposition != WindowOpenDisposition::NEW_WINDOW);
  LiveTabContextBrowserAgent* context =
      LiveTabContextBrowserAgent::FromBrowser(browser);
  // Passing a nil context into RestoreEntryById can result in the restore
  // service requesting a new window. This is unsupported on iOS (see above
  // TODO).
  DCHECK(context);
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForProfile(profile);
  restoreService->RestoreEntryById(context, session_id, disposition);
}

UrlLoadParams CreateOmniboxUrlLoadParams(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool destination_url_entered_without_scheme,
    bool is_incognito) {
  transition = ui::PageTransitionFromInt(transition |
                                         ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  web::NavigationManager::WebLoadParams web_params =
      web_navigation_util::CreateWebLoadParams(destination_url, transition,
                                               post_content);
  if (destination_url_entered_without_scheme) {
    web_params.https_upgrade_type = web::HttpsUpgradeType::kOmnibox;
  }
  NSMutableDictionary<NSString*, NSString*>* combinedExtraHeaders =
      [web_navigation_util::VariationHeadersForURL(destination_url,
                                                   is_incognito) mutableCopy];
  [combinedExtraHeaders addEntriesFromDictionary:web_params.extra_headers];
  web_params.extra_headers = [combinedExtraHeaders copy];
  UrlLoadParams params = UrlLoadParams::InCurrentTab(web_params);
  params.disposition = disposition;
  return params;
}
