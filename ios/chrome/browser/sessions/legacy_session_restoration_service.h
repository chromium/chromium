// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_LEGACY_SESSION_RESTORATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_LEGACY_SESSION_RESTORATION_SERVICE_H_

#include <set>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/session_restoration_service.h"
#include "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"

@class SessionServiceIOS;
@class WebSessionStateCache;
namespace sessions {
class TabRestoreService;
}  // namespace sessions

// Implementation of SessionRestorationService that wraps the legacy API
// (SessionRestorationBrowserAgent and SessionServiceIOS). Used when the
// feature is disabled.
//
// TODO(crbug.com/1383087): Remove when the feature is fully launched.
class LegacySessionRestorationService final : public SessionRestorationService,
                                              public SessionRestorationObserver,
                                              public WebStateListObserver {
 public:
  LegacySessionRestorationService(
      bool is_pinned_tabs_enabled,
      const base::FilePath& storage_path,
      SessionServiceIOS* session_service_ios,
      WebSessionStateCache* web_session_state_cache,
      sessions::TabRestoreService* tab_restore_service);

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
  void Disconnect(Browser* browser) final;
  std::unique_ptr<web::WebState> CreateUnrealizedWebState(
      Browser* browser,
      web::proto::WebStateStorage storage) final;
  void DeleteDataForDiscardedSessions(const std::set<std::string>& identifiers,
                                      base::OnceClosure closure) final;
  void InvokeClosureWhenBackgroundProcessingDone(
      base::OnceClosure closure) final;
  void PurgeUnassociatedData(base::OnceClosure closure) final;

  // SessionRestorationObserver implementation.
  void WillStartSessionRestoration(Browser* browser) final;
  void SessionRestorationFinished(
      Browser* browser,
      const std::vector<web::WebState*>& restored_web_states) final;

  // WebStateListObserver implementation.
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) final;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Observer list.
  base::ObserverList<SessionRestorationObserver, true> observers_;

  // Whether pinned tabs support is enabled (injected via the constructor to
  // allow easily testing code controlled by this boolean independently of
  // whether the feature is enabled in the application).
  const bool is_pinned_tabs_enabled_;

  // Root directory in which the data should be written to or loaded from.
  const base::FilePath storage_path_;

  // Service used to schedule and save the data to storage.
  __strong SessionServiceIOS* session_service_ios_ = nil;

  // Service used to manage WKWebView native session storage.
  __strong WebSessionStateCache* web_session_state_cache_ = nil;

  // Pointer to the TabRestoreService used to report closed tabs if the
  // session migration fails.
  raw_ptr<sessions::TabRestoreService> tab_restore_service_ = nullptr;

  // Set of observed Browser objects.
  std::set<Browser*> browsers_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_LEGACY_SESSION_RESTORATION_SERVICE_H_
