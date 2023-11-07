// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/legacy_session_restoration_service.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

LegacySessionRestorationService::LegacySessionRestorationService(
    bool is_pinned_tabs_enabled,
    SessionServiceIOS* session_service_ios)
    : is_pinned_tabs_enabled_(is_pinned_tabs_enabled),
      session_service_ios_(session_service_ios) {
  DCHECK(session_service_ios_);
}

LegacySessionRestorationService::~LegacySessionRestorationService() {}

void LegacySessionRestorationService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browsers_.empty()) << "Disconnect() must be called for all Browser";
}

void LegacySessionRestorationService::AddObserver(
    SessionRestorationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void LegacySessionRestorationService::RemoveObserver(
    SessionRestorationObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void LegacySessionRestorationService::SaveSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Browser* browser : browsers_) {
    SessionRestorationBrowserAgent::FromBrowser(browser)->SaveSession(
        /* immediately*/ true);
  }
}

void LegacySessionRestorationService::ScheduleSaveSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Browser* browser : browsers_) {
    SessionRestorationBrowserAgent::FromBrowser(browser)->SaveSession(
        /* immediately*/ false);
  }
}

void LegacySessionRestorationService::SetSessionID(
    Browser* browser,
    const std::string& identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(browsers_, browser));
  browsers_.insert(browser);

  // Create the SessionRestorationBrowserAgent for browser.
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser, session_service_ios_, is_pinned_tabs_enabled_);

  SessionRestorationBrowserAgent* browser_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);

  browser_agent->AddObserver(this);
  browser_agent->SetSessionID(base::SysUTF8ToNSString(identifier));
}

void LegacySessionRestorationService::LoadSession(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(browsers_, browser));
  SessionRestorationBrowserAgent::FromBrowser(browser)->RestoreSession();
}

void LegacySessionRestorationService::Disconnect(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(browsers_, browser));
  browsers_.erase(browser);

  SessionRestorationBrowserAgent* browser_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);

  browser_agent->SaveSession(/* immediately */ true);
  browser_agent->RemoveObserver(this);

  // Destroy the SessionRestorationBrowserAgent for browser.
  SessionRestorationBrowserAgent::RemoveFromBrowser(browser);
}

std::unique_ptr<web::WebState>
LegacySessionRestorationService::CreateUnrealizedWebState(
    Browser* browser,
    web::proto::WebStateStorage storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web::WebState::CreateWithStorageSession(
      web::WebState::CreateParams(browser->GetBrowserState()),
      [[CRWSessionStorage alloc] initWithProto:storage]);
}

void LegacySessionRestorationService::InvokeClosureWhenBackgroundProcessingDone(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [session_service_ios_ shutdownWithClosure:std::move(closure)];
}

void LegacySessionRestorationService::WillStartSessionRestoration(
    Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SessionRestorationObserver& observer : observers_) {
    observer.WillStartSessionRestoration(browser);
  }
}

void LegacySessionRestorationService::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (SessionRestorationObserver& observer : observers_) {
    observer.SessionRestorationFinished(browser, restored_web_states);
  }
}
