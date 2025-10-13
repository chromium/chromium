// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_LEGACY_SESSION_RESTORATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_LEGACY_SESSION_RESTORATION_SERVICE_H_

#include <MacTypes.h>

#include <map>
#include <set>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/model/session_restoration_service.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#include "ios/web/public/web_state_observer.h"

@class SessionServiceIOS;
@class WebSessionStateCache;

// Implementation of SessionRestorationService that wraps the legacy API
// (SessionRestorationBrowserAgent and SessionServiceIOS). Used when the
// feature is disabled.
//
// TODO(crbug.com/40945317): Remove when the feature is fully launched.
class LegacySessionRestorationService final : public SessionRestorationService,
                                              public SessionRestorationObserver,
                                              public WebStateListObserver,
                                              public web::WebStateObserver {
 public:
  LegacySessionRestorationService(
      bool enable_pinned_tabs,
      const base::FilePath& storage_path,
      SessionServiceIOS* session_service_ios,
      WebSessionStateCache* web_session_state_cache);

  ~LegacySessionRestorationService() final;

  // KeyedService implementation.
  void Shutdown() final;

  // SessionRestorationService implementation.
  void AddObserver(SessionRestorationObserver* observer) final;
  void RemoveObserver(SessionRestorationObserver* observer) final;
  void SaveSessions() final;
  void ScheduleSaveSessions() final;
  void SetSessionID(Browser* browser, const std::string& identifier) final;
  void LoadSession(Browser* browser) final;
  void LoadWebStateStorage(Browser* browser,
                           web::WebState* web_state,
                           WebStateStorageCallback callback) final;
  void AttachBackup(Browser* browser, Browser* backup) final;
  void Disconnect(Browser* browser) final;
  std::unique_ptr<web::WebState> CreateUnrealizedWebState(
      Browser* browser,
      web::proto::WebStateStorage storage) final;
  void DeleteDataForDiscardedSessions(const std::set<std::string>& identifiers,
                                      base::OnceClosure closure) final;
  void InvokeClosureWhenBackgroundProcessingDone(
      base::OnceClosure closure) final;
  void PurgeUnassociatedData(base::OnceClosure closure) final;
  void ParseDataForBrowserAsync(
      Browser* browser,
      WebStateStorageIterationCallback iter_callback,
      WebStateStorageIterationCompleteCallback done) final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;

  // web::WebStateObserver implementation.
  void WebStateRealized(web::WebState* web_state) final;
  void WebStateDestroyed(web::WebState* web_state) final;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Invoked when a WebState is inserted/removed from a WebStateList.
  void WebStateInserted(web::WebState* web_state);
  void WebStateDetached(web::WebState* web_state);

  // Observer list.
  base::ObserverList<SessionRestorationObserver, true> observers_;

  // Whether features support is enabled (injected via the constructor to
  // allow easily testing code controlled by this boolean independently of
  // whether the feature is enabled in the application).
  const bool enable_pinned_tabs_;

  // Root directory in which the data should be written to or loaded from.
  const base::FilePath storage_path_;

  // Service used to schedule and save the data to storage.
  __strong SessionServiceIOS* session_service_ios_ = nil;

  // Service used to manage WKWebView native session storage.
  __strong WebSessionStateCache* web_session_state_cache_ = nil;

  // Set of observed Browser objects.
  std::set<Browser*> browsers_;

  // Bi-directional mapping of observed Browser and their backup.
  std::map<Browser*, Browser*> browsers_to_backup_;
  std::map<Browser*, Browser*> backups_to_browser_;

  // Used to observe unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_LEGACY_SESSION_RESTORATION_SERVICE_H_
