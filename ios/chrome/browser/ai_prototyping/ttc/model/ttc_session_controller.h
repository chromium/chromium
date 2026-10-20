// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/memory/weak_ptr.h"
#import "base/observer_list.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "components/ttc/app/public/error_codes.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_conversation.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_session_controller_observer.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_voice_focused_web_state_tracker.h"

class BrowserList;

namespace ttc {
class TtcBackend;
}  // namespace ttc

class TTCJournal;

namespace web {
class WebState;
}  // namespace web

// Coordinates the lifecycle, observer notifications, and backgrounding teardown
// for an active TTC session on iOS.
class TtcSessionController : public TtcConversation::Delegate,
                             public TtcVoiceFocusedWebStateTracker::Observer,
                             public TtcPageContextMonitor::Delegate {
 public:
  // Underlying conversation coordinator.
  TtcConversation* conversation() const { return conversation_.get(); }

  // Current lifecycle state of this session.
  TTCSessionLifecycle lifecycle() const { return lifecycle_; }

  // Optional session journal for structured event logging.
  TTCJournal* journal() const;
  void set_journal(TTCJournal* journal);

  // Returns the active `WebState` currently tracked by this session, or
  // `nullptr` if none.
  web::WebState* GetVoiceFocusedWebState() const;

  // Constructs a session controller injecting a custom `conversation`
  // coordinator and optional `voice_focused_web_state_tracker`. When
  // `voice_focused_web_state_tracker` is `nullptr` (e.g., in unit tests),
  // page context tracking is disabled.
  explicit TtcSessionController(std::unique_ptr<TtcConversation> conversation,
                                std::unique_ptr<TtcVoiceFocusedWebStateTracker>
                                    voice_focused_web_state_tracker = nullptr);

  // Constructs a session controller creating a `TtcConversation` with
  // `backend`, optionally monitoring the active `WebState` of `browser_list`
  // (or none if `nullptr`).
  explicit TtcSessionController(std::unique_ptr<ttc::TtcBackend> backend,
                                BrowserList* browser_list = nullptr);

  // Constructs a session controller using a default `TtcConversation`.
  TtcSessionController();

  TtcSessionController(const TtcSessionController&) = delete;
  TtcSessionController& operator=(const TtcSessionController&) = delete;
  ~TtcSessionController() override;

#pragma mark - Session Lifecycle

  // Starts the session.
  void StartSession();

  // Transitions the session lifecycle to `kFinished`.
  void StopSession();

  // Stops the session, unregisters background observers, and removes all
  // registered observers.
  void Disconnect();

#pragma mark - Observers

  // Adds an observer to receive session events.
  void AddObserver(TtcSessionControllerObserver* observer);

  // Removes an observer from receiving session events.
  void RemoveObserver(TtcSessionControllerObserver* observer);

  // Returns a weak pointer to this controller.
  base::WeakPtr<TtcSessionController> GetWeakPtr();

  // `TtcConversation::Delegate` implementation:
  void OnConversationInitialized() override;
  void OnConversationClosed() override;
  void OnAudioEnergyUpdated(float energy) override;
  void OnConversationError(ttc::ErrorCode error) override;

  // `TtcVoiceFocusedWebStateTracker::Observer` implementation:
  void OnVoiceFocusedWebStateChanged(web::WebState* web_state) override;

  // `TtcPageContextMonitor::Delegate` implementation:
  void OnPageContextFetched(
      PageContextWrapperCallbackResponse response) override;

 private:
  friend class TtcSessionControllerTest;

  // Updates `lifecycle_` and notifies registered observers if the state
  // changed.
  void SetLifecycle(TTCSessionLifecycle lifecycle);

  // Registers for `UIApplicationDidEnterBackgroundNotification` to stop the
  // session when the app enters the background.
  void RegisterBackgroundObserver();

  // Handles `UIApplicationDidEnterBackgroundNotification`.
  void ApplicationDidEnterBackground();

  // Unregisters `background_observer_` from `NSNotificationCenter`.
  void RemoveBackgroundObserver();

  // Journal logging helpers.
  void LogLifecycleTransition(TTCSessionLifecycle current_lifecycle,
                              TTCSessionLifecycle new_lifecycle);
  void LogSessionError(ttc::ErrorCode error);
  void LogAppDidEnterBackground();

  // Underlying conversation coordinator owned by this session controller.
  std::unique_ptr<TtcConversation> conversation_;

  // Tracker following the active `WebState` across the profile's regular
  // `Browser`s. May be `nullptr` in unit tests constructed without a
  // `BrowserList`.
  std::unique_ptr<TtcVoiceFocusedWebStateTracker>
      voice_focused_web_state_tracker_;

  // Scoped observation of `voice_focused_web_state_tracker_`.
  base::ScopedObservation<TtcVoiceFocusedWebStateTracker,
                          TtcVoiceFocusedWebStateTracker::Observer>
      voice_focused_web_state_tracker_observation_{this};

  // Per-tab monitor extracting `AnnotatedPageContent` for the active
  // `WebState` tracked by this session.
  std::unique_ptr<TtcPageContextMonitor> page_context_monitor_;

  // Current lifecycle state of the session.
  TTCSessionLifecycle lifecycle_ = TTCSessionLifecycle::kInitializing;

  // Registered observers receiving session lifecycle, audio level, and error
  // updates.
  base::ObserverList<TtcSessionControllerObserver> observers_;

  // Whether a fatal error has already been reported during this session.
  bool fatal_error_reported_ = false;

  // Notification token for `UIApplicationDidEnterBackgroundNotification`.
  id<NSObject> background_observer_ = nil;

  // Optional session journal for structured event logging.
  base::WeakPtr<TTCJournal> journal_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<TtcSessionController> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_SESSION_CONTROLLER_H_
