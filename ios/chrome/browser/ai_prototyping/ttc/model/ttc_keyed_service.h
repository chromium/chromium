// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_H_

#import <memory>

#import "base/callback_list.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ptr.h"
#import "base/memory/weak_ptr.h"
#import "base/sequence_checker.h"
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_journal.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

class BrowserList;
class OptimizationGuideService;
class TtcSessionController;

// Profile-keyed service that manages the active TTC voice session
// lifecycle, state transitions, and coordination for a Profile.
class TTCKeyedService : public KeyedService {
 public:
  // Constructs the keyed service with `optimization_guide` and `browser_list`
  // for the owning profile.
  TTCKeyedService(OptimizationGuideService* optimization_guide,
                  BrowserList* browser_list);
  TTCKeyedService(const TTCKeyedService&) = delete;
  TTCKeyedService& operator=(const TTCKeyedService&) = delete;
  ~TTCKeyedService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Returns true if TTC is enabled for this profile.
  bool IsEnabled() const;

  // Returns the current `TTCServiceState` for this profile.
  TTCServiceState GetState() const;

  // Returns the current `TTCSessionLifecycle` for the active session, or
  // `kFinished` if no session is active.
  TTCSessionLifecycle GetSessionLifecycle() const;

  // Starts an active voice session. Crashes if a session is already active
  // or if TTC is disabled.
  void StartSession();

  // Ends the active session. This is a no-op if no session is active.
  void EndSession();

  // Returns true if a voice session is currently active.
  bool is_session_active() const;

  // Returns the active `TtcSessionController`, or nullptr if no session is
  // active.
  TtcSessionController* session_controller() const;

  // Returns the shared session journal for this service. Entries persist across
  // `EndSession()` and are cleared on the next `StartSession()`.
  TTCJournal& journal() { return journal_; }
  const TTCJournal& journal() const { return journal_; }

  // Registers a callback to be notified whenever `TTCServiceState` changes.
  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingCallback<void(TTCServiceState)> callback);

  // Returns a weak pointer to this service instance.
  base::WeakPtr<TTCKeyedService> GetWeakPtr();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // The OptimizationGuideService this service is using.
  raw_ptr<OptimizationGuideService> optimization_guide_ = nullptr;

  // The `BrowserList` for the owning profile, passed to session controllers.
  raw_ptr<BrowserList> browser_list_ = nullptr;

  // Shared session journal retaining entries for the current or most recent
  // session. Declared before `session_controller_` so `journal_` outlives any
  // active session controller referencing it.
  TTCJournal journal_;

  // Active session coordinator, or nullptr when no session is in progress.
  std::unique_ptr<TtcSessionController> session_controller_;

  // List of callbacks notified when `TTCServiceState` changes.
  base::RepeatingCallbackList<void(TTCServiceState)> state_changed_callbacks_;

  base::WeakPtrFactory<TTCKeyedService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_H_
