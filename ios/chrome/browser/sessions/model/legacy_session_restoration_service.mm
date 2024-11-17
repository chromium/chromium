// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/legacy_session_restoration_service.h"

#import "base/check.h"
#import "base/containers/contains.h"
#import "base/functional/callback.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/sessions/model/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/model/session_service_ios.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"
#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state.h"

namespace {

// An output iterator that counts how many time it has been incremented.
// Allows to check if sets has non-empty intersection without allocating.
template <typename T1, typename T2>
struct CountingOutputIterator {
  CountingOutputIterator& operator++() {
    ++count;
    return *this;
  }
  CountingOutputIterator& operator++(int) {
    ++count;
    return *this;
  }

  CountingOutputIterator& operator*() { return *this; }
  CountingOutputIterator& operator=(const T1&) { return *this; }
  CountingOutputIterator& operator=(const T2&) { return *this; }

  uint32_t count = 0;
};

// Override of CountingOutputIterator<T1, T2> when types are identical.
template <typename T>
struct CountingOutputIterator<T, T> {
  CountingOutputIterator& operator++() {
    ++count;
    return *this;
  }
  CountingOutputIterator& operator++(int) {
    ++count;
    return *this;
  }

  CountingOutputIterator& operator*() { return *this; }
  CountingOutputIterator& operator=(const T&) { return *this; }

  uint32_t count = 0;
};

// Returns whether the two sets have non-empty intersection.
template <typename Range1, typename Range2>
constexpr bool HasIntersection(Range1&& range1, Range2&& range2) {
  auto result = base::ranges::set_intersection(
      std::forward<Range1>(range1), std::forward<Range2>(range2),
      CountingOutputIterator<decltype(*range1.begin()),
                             decltype(*range2.begin())>{});
  return result.count != 0;
}

// Returns the sessions identifiers for all `browsers`.
std::set<std::string> GetSessionIdentifiers(
    const std::set<Browser*>& browsers) {
  std::set<std::string> result;
  for (const Browser* browser : browsers) {
    result.insert(base::SysNSStringToUTF8(
        SessionRestorationBrowserAgent::FromBrowser(browser)->GetSessionID()));
  }
  return result;
}

}  // namespace

LegacySessionRestorationService::LegacySessionRestorationService(
    bool enable_pinned_tabs,
    bool enable_tab_groups,
    const base::FilePath& storage_path,
    SessionServiceIOS* session_service_ios,
    WebSessionStateCache* web_session_state_cache)
    : enable_pinned_tabs_(enable_pinned_tabs),
      enable_tab_groups_(enable_tab_groups),
      storage_path_(storage_path),
      session_service_ios_(session_service_ios),
      web_session_state_cache_(web_session_state_cache) {
  DCHECK(session_service_ios_);
  DCHECK(web_session_state_cache_);
}

LegacySessionRestorationService::~LegacySessionRestorationService() {}

void LegacySessionRestorationService::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(browsers_.empty()) << "Disconnect() must be called for all Browser";

  [session_service_ios_ shutdown];
  session_service_ios_ = nil;

  web_session_state_cache_ = nil;
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
    // SessionRestorationBrowserAgent is not created for backup Browsers.
    if (SessionRestorationBrowserAgent* browser_agent =
            SessionRestorationBrowserAgent::FromBrowser(browser)) {
      browser_agent->SaveSession(/*immediately=*/true);
    }
  }
}

void LegacySessionRestorationService::ScheduleSaveSessions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (Browser* browser : browsers_) {
    // SessionRestorationBrowserAgent is not created for backup Browsers.
    if (SessionRestorationBrowserAgent* browser_agent =
            SessionRestorationBrowserAgent::FromBrowser(browser)) {
      browser_agent->SaveSession(/*immediately=*/false);
    }
  }
}

void LegacySessionRestorationService::SetSessionID(
    Browser* browser,
    const std::string& identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(browsers_, browser));
  browsers_.insert(browser);

  browser->GetWebStateList()->AddObserver(this);

  // Create the SessionRestorationBrowserAgent for browser.
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser, session_service_ios_, enable_pinned_tabs_, enable_tab_groups_);

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

void LegacySessionRestorationService::LoadWebStateStorage(
    Browser* browser,
    web::WebState* web_state,
    WebStateStorageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::Contains(browsers_, browser)) {
    return;
  }

  web::proto::WebStateStorage storage;
  [web_state->BuildSessionStorage() serializeToProto:storage];
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(storage)));
}

void LegacySessionRestorationService::AttachBackup(Browser* browser,
                                                   Browser* backup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(browsers_, browser));
  DCHECK(!base::Contains(browsers_, backup));
  DCHECK(!base::Contains(browsers_to_backup_, browser));

  browsers_.insert(backup);
  browsers_to_backup_.insert(std::make_pair(browser, backup));
  backups_to_browser_.insert(std::make_pair(backup, browser));
}

void LegacySessionRestorationService::Disconnect(Browser* browser) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(browsers_, browser));
  browsers_.erase(browser);

  // Deal with backup Browser.
  if (base::Contains(backups_to_browser_, browser)) {
    Browser* original = backups_to_browser_[browser];
    backups_to_browser_.erase(browser);
    browsers_to_backup_.erase(original);
    return;
  }

  // Must disconnect the backup Browser before the original Browser.
  DCHECK(!base::Contains(browsers_to_backup_, browser));

  SessionRestorationBrowserAgent* browser_agent =
      SessionRestorationBrowserAgent::FromBrowser(browser);

  browser_agent->SaveSession(/* immediately */ true);
  browser_agent->RemoveObserver(this);

  // Destroy the SessionRestorationBrowserAgent for browser.
  SessionRestorationBrowserAgent::RemoveFromBrowser(browser);

  browser->GetWebStateList()->RemoveObserver(this);
}

std::unique_ptr<web::WebState>
LegacySessionRestorationService::CreateUnrealizedWebState(
    Browser* browser,
    web::proto::WebStateStorage storage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web::WebState::CreateWithStorageSession(
      web::WebState::CreateParams(browser->GetProfile()),
      [[CRWSessionStorage alloc] initWithProto:storage
                              uniqueIdentifier:web::WebStateID::NewUnique()
                              stableIdentifier:[[NSUUID UUID] UUIDString]],
      base::ReturnValueOnce<NSData*>(nil));
}

void LegacySessionRestorationService::DeleteDataForDiscardedSessions(
    const std::set<std::string>& identifiers,
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!HasIntersection(identifiers, GetSessionIdentifiers(browsers_)));
  NSMutableArray<NSString*>* sessions = [[NSMutableArray alloc] init];
  for (const std::string& identifier : identifiers) {
    [sessions addObject:base::SysUTF8ToNSString(identifier)];
  }
  [session_service_ios_ deleteSessions:sessions
                             directory:storage_path_
                            completion:std::move(closure)];
}

void LegacySessionRestorationService::InvokeClosureWhenBackgroundProcessingDone(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [session_service_ios_ shutdownWithClosure:std::move(closure)];
}

void LegacySessionRestorationService::PurgeUnassociatedData(
    base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  [web_session_state_cache_
      purgeUnassociatedDataWithCompletion:std::move(closure)];
}

void LegacySessionRestorationService::ParseDataForBrowserAsync(
    Browser* browser,
    WebStateStorageIterationCallback iter_callback,
    WebStateStorageIterationCompleteCallback done) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(base::Contains(browsers_, browser));
  using SessionMap = std::map<web::WebStateID, CRWSessionStorage*>;
  SessionMap sessions;

  // Collect the data synchronously (since it is available when using the
  // legacy implementation).
  WebStateList* const web_state_list = browser->GetWebStateList();
  const int web_state_list_count = web_state_list->count();
  for (int index = 0; index < web_state_list_count; ++index) {
    web::WebState* const web_state = web_state_list->GetWebStateAt(index);
    web::WebStateID const web_state_id = web_state->GetUniqueIdentifier();
    sessions.insert(
        std::make_pair(web_state_id, web_state->BuildSessionStorage()));
  }

  // Post a task that will invoke `iter_callback` for all WebState and
  // then `done`.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](SessionMap sessions, WebStateStorageIterationCallback iterator) {
            for (const auto& [web_state_id, session] : sessions) {
              web::proto::WebStateStorage storage;
              [session serializeToProto:storage];
              iterator.Run(web_state_id, std::move(storage));
            }
          },
          std::move(sessions), std::move(iter_callback))
          .Then(std::move(done)));
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

void LegacySessionRestorationService::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (change.type()) {
    case WebStateListChange::Type::kInsert: {
      const auto& typed_change = change.As<WebStateListChangeInsert>();
      WebSessionStateTabHelper::CreateForWebState(
          typed_change.inserted_web_state());
      break;
    }

    case WebStateListChange::Type::kReplace: {
      const auto& typed_change = change.As<WebStateListChangeReplace>();
      WebSessionStateTabHelper::CreateForWebState(
          typed_change.inserted_web_state());
      break;
    }

    default:
      break;
  }
}
