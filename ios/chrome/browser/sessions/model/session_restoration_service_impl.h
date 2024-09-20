// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_IMPL_H_

#include <map>
#include <set>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ios/chrome/browser/sessions/model/session_io_request.h"
#include "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/model/session_restoration_service.h"

class WebStateList;

// Concrete implementation of the SessionRestorationService.
//
// After a Browser is passed to `SetSessionID()` the service will observe
// any modification affecting it and the WebState it contains. Any time
// significant changes are detected, the service will schedule a task to
// write down those changes to storage.
class SessionRestorationServiceImpl final : public SessionRestorationService {
 public:
  SessionRestorationServiceImpl(
      base::TimeDelta save_delay,
      bool enable_pinned_web_states,
      bool enable_tab_groups,
      const base::FilePath& storage_path,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  ~SessionRestorationServiceImpl() final;

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

 private:
  // Helper type used to record information about an orphaned WebState.
  struct OrphanInfo;

  // Helper type used to record information about a single WebStateList.
  class WebStateListInfo;

  // Invoked when changes are detected in `web_state_list` or any of its
  // contained WebStates (see SessionRestorationWebStateListObserver for
  // details).
  void MarkWebStateListDirty(WebStateList* web_state_list);

  // Updates the map of orphaned WebStates.
  void UpdateOrphanInfoMap();

  // Helper method that post a task to save state to storage.
  void SaveDirtySessions();

  // Used to enforce use on the correct sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Observer list.
  base::ObserverList<SessionRestorationObserver, true> observers_;

  // Delay before saving data to disk after a change is detected (injected
  // via the constructor to allow easily testing this object).
  const base::TimeDelta save_delay_;

  // Whether features support is enabled (injected via the constructor to
  // allow easily testing code controlled by this boolean independently of
  // whether the feature is enabled in the application).
  const bool enable_pinned_web_states_;
  const bool enable_tab_groups_;

  // Root directory in which the data should be written to or loaded from.
  const base::FilePath storage_path_;

  // Task runner used to perform background actions.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Maps from observed WebStateList to the object tracking the information
  // about said WebStateList (including the observer).
  std::map<WebStateList*, std::unique_ptr<WebStateListInfo>> infos_;

  // Set of WebStateLists that are considered dirty and whose state needs to
  // be (at least partially) saved to storage in `SaveDirtySessions()`.
  std::set<WebStateList*> dirty_web_state_lists_;

  // Used to enforce that the identifier are not shared between Browser.
  std::set<std::string> identifiers_;

  // Information about orphaned WebStates.
  std::map<web::WebStateID, OrphanInfo> orphaned_map_;

  // List of pending requests from CreateUnrealizedWebState(...).
  ios::sessions::IORequestList pending_requests_;

  // Timer used to delay and batch saving data to storage.
  base::OneShotTimer timer_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_RESTORATION_SERVICE_IMPL_H_
