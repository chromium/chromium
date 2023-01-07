// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/certificate_policy_app_agent.h"

#import "base/task/cancelable_task_tracker.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Updates `policy_cache` by adding entries from the session policy cache in
// `web_state`.
void UpdateCertificatePolicyCacheFromWebState(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    const web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // The WebState install its certificate policy cache upon realization, so
  // unrealized WebState can be skipped (to avoid forcing their realization).
  if (!web_state->IsRealized())
    return;

  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
      policy_cache);
}

// Populates the certificate policy cache based on all of the WebStates in
// the `incognito` browsers in `browser_list`. Because this is called
// asynchronously, it needs to be resilient to shutdown having happened before
// it is invoked.
void RestoreCertificatePolicyCacheFromBrowsers(
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    BrowserList* browser_list,
    bool incognito) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // If the browser list is shutdown, it's too late to do anything.
  if (browser_list->IsShutdown())
    return;

  std::set<Browser*> browsers = incognito ? browser_list->AllIncognitoBrowsers()
                                          : browser_list->AllRegularBrowsers();

  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int index = 0; index < web_state_list->count(); ++index) {
      UpdateCertificatePolicyCacheFromWebState(
          policy_cache, web_state_list->GetWebStateAt(index));
    }
  }
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current `incognito` browsers in `browser_list`.
// Clearing the cache is done on the IO thread, and then cache repopulation is
// done on the UI thread.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    BrowserList* browser_list,
    bool incognito) {
  DCHECK(policy_cache);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&web::CertificatePolicyCache::ClearCertificatePolicies,
                     policy_cache),
      base::BindOnce(&RestoreCertificatePolicyCacheFromBrowsers, policy_cache,
                     browser_list, incognito));
}

}  // anonymous namespace

@implementation CertificatePolicyAppAgent {
  // Used to ensure thread-safety of the certificate policy management code.
  base::CancelableTaskTracker _clearPoliciesTaskTracker;
}

- (void)dealloc {
  // Clean up any remaining tasks.
  _clearPoliciesTaskTracker.TryCancelAll();
}

- (BOOL)isWorking {
  return static_cast<BOOL>(_clearPoliciesTaskTracker.HasTrackedTasks());
}

- (void)appDidEnterBackground {
  ChromeBrowserState* browserState = self.appState.mainBrowserState;
  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(browserState);

  // Evict all the certificate policies except for the current entries of the
  // active sessions, for the regular and incognito browsers.
  CleanCertificatePolicyCache(
      &_clearPoliciesTaskTracker, web::GetIOThreadTaskRunner({}),
      web::BrowserState::GetCertificatePolicyCache(browserState), browserList,
      /*incognito=*/false);

  if (browserState->HasOffTheRecordChromeBrowserState()) {
    ChromeBrowserState* incognitoBrowserState =
        browserState->GetOffTheRecordChromeBrowserState();
    CleanCertificatePolicyCache(
        &_clearPoliciesTaskTracker, web::GetIOThreadTaskRunner({}),
        web::BrowserState::GetCertificatePolicyCache(incognitoBrowserState),
        browserList, /*incognito=*/true);
  }
}

@end
