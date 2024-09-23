// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/certificate_policy_app_agent.h"

#import "base/task/cancelable_task_tracker.h"
#import "base/task/single_thread_task_runner.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_manager_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"

namespace {

// Updates the BrowserState's policy cache from the `web_state` session policy
// cache.
void UpdateCertificatePolicyCacheFromWebState(const web::WebState* web_state) {
  DCHECK(web_state);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);

  // The WebState install its certificate policy cache upon realization, so
  // unrealized WebState can be skipped (to avoid forcing their realization).
  if (!web_state->IsRealized())
    return;

  web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache();
}

// Populates the certificate policy cache based on all of the WebStates in
// the `incognito` browsers of `weak_profile`. Because this is called
// asynchronously, it needs to be resilient to shutdown having happened before
// it is invoked.
void RestoreCertificatePolicyCacheFromBrowsers(
    base::WeakPtr<ProfileIOS> weak_profile,
    bool incognito) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  // If the ProfileIOS is destroyed, it's too late to do anything.
  ProfileIOS* profile = weak_profile.get();
  if (!profile) {
    return;
  }

  BrowserList* browser_list = BrowserListFactory::GetForProfile(profile);

  const BrowserList::BrowserType browser_types =
      incognito ? BrowserList::BrowserType::kIncognito
                : BrowserList::BrowserType::kRegularAndInactive;
  std::set<Browser*> browsers = browser_list->BrowsersOfType(browser_types);

  for (Browser* browser : browsers) {
    WebStateList* web_state_list = browser->GetWebStateList();
    for (int index = 0; index < web_state_list->count(); ++index) {
      UpdateCertificatePolicyCacheFromWebState(
          web_state_list->GetWebStateAt(index));
    }
  }
}

// Scrubs the certificate policy cache of all certificates policies except
// those for the current `incognito` browsers in `weak_profile`.
// Clearing the cache is done on the IO thread, and then cache repopulation is
// done on the UI thread.
void CleanCertificatePolicyCache(
    base::CancelableTaskTracker* task_tracker,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const scoped_refptr<web::CertificatePolicyCache>& policy_cache,
    base::WeakPtr<ProfileIOS> weak_profile,
    bool incognito) {
  DCHECK(policy_cache);
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  task_tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&web::CertificatePolicyCache::ClearCertificatePolicies,
                     policy_cache),
      base::BindOnce(&RestoreCertificatePolicyCacheFromBrowsers,
                     std::move(weak_profile), incognito));
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
  for (ProfileIOS* profile :
       GetApplicationContext()->GetProfileManager()->GetLoadedProfiles()) {
    // Evict all the certificate policies except for the current entries of the
    // active sessions, for the regular and incognito browsers.
    CleanCertificatePolicyCache(
        &_clearPoliciesTaskTracker, web::GetIOThreadTaskRunner({}),
        web::BrowserState::GetCertificatePolicyCache(profile),
        profile->AsWeakPtr(),
        /*incognito=*/false);

    if (profile->HasOffTheRecordProfile()) {
      ProfileIOS* incognitoBrowserState = profile->GetOffTheRecordProfile();
      CleanCertificatePolicyCache(
          &_clearPoliciesTaskTracker, web::GetIOThreadTaskRunner({}),
          web::BrowserState::GetCertificatePolicyCache(incognitoBrowserState),
          profile->AsWeakPtr(), /*incognito=*/true);
    }
  }
}

@end
