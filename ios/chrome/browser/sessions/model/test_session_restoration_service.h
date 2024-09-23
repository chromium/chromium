// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_SERVICE_H_

#include "base/observer_list.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/model/session_restoration_service.h"

// A test implementation of SessionRestorationService.
//
// This test implementation does not support saving or loading session,
// but correctly implements the SessionRestoration API.
class TestSessionRestorationService : public SessionRestorationService {
 public:
  TestSessionRestorationService();
  ~TestSessionRestorationService() override;

  // Returns a callback that can be used as a TestingFactory for KeyedService
  // infrastructure.
  static BrowserStateKeyedServiceFactory::TestingFactory GetTestingFactory();

  // SessionRestorationService implementation.
  void AddObserver(SessionRestorationObserver* observer) override;
  void RemoveObserver(SessionRestorationObserver* observer) override;
  void SaveSessions() override;
  void ScheduleSaveSessions() override;
  void SetSessionID(Browser* browser, const std::string& identifier) override;
  void LoadSession(Browser* browser) override;
  void LoadWebStateStorage(Browser* browser,
                           web::WebState* web_state,
                           WebStateStorageCallback callback) override;
  void AttachBackup(Browser* browser, Browser* backup) final;
  void Disconnect(Browser* browser) override;
  std::unique_ptr<web::WebState> CreateUnrealizedWebState(
      Browser* browser,
      web::proto::WebStateStorage storage) override;
  void DeleteDataForDiscardedSessions(const std::set<std::string>& identifiers,
                                      base::OnceClosure closure) override;
  void InvokeClosureWhenBackgroundProcessingDone(
      base::OnceClosure closure) override;
  void PurgeUnassociatedData(base::OnceClosure closure) final;
  void ParseDataForBrowserAsync(
      Browser* browser,
      WebStateStorageIterationCallback iter_callback,
      WebStateStorageIterationCompleteCallback done) final;

 private:
  base::ObserverList<SessionRestorationObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_TEST_SESSION_RESTORATION_SERVICE_H_
