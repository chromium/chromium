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
#import "components/keyed_service/core/keyed_service.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_states.h"

class ProfileIOS;

// Profile-keyed service that manages the active TalkToChrome voice session
// lifecycle, state transitions, and coordination for a Profile.
class TTCKeyedService : public KeyedService {
 public:
  // Returns the `TTCKeyedService` associated with `profile`, or nullptr
  // if disabled.
  static TTCKeyedService* Get(ProfileIOS* profile);

  explicit TTCKeyedService(ProfileIOS* profile);
  TTCKeyedService(const TTCKeyedService&) = delete;
  TTCKeyedService& operator=(const TTCKeyedService&) = delete;
  ~TTCKeyedService() override;

  // KeyedService implementation:
  void Shutdown() override;

  // Returns true if TTC is enabled for this profile.
  bool IsEnabled() const;

  // Returns the current `TTCServiceState` for this profile.
  TTCServiceState GetState() const;

  // Starts an active voice session. Crashes if a session is already active
  // or if TTC is disabled.
  void StartSession();

  // Ends the active session. This is a no-op if no session is active.
  void EndSession();

  // Returns true if a voice session is currently active.
  bool is_session_active() const { return is_session_active_; }

  // Registers a callback to be notified whenever `TTCServiceState` changes.
  base::CallbackListSubscription RegisterStateChangedCallback(
      base::RepeatingCallback<void(TTCServiceState)> callback);

  // Returns the `ProfileIOS` associated with this service.
  ProfileIOS* profile() const { return profile_; }

  // Returns a weak pointer to this service instance.
  base::WeakPtr<TTCKeyedService> GetWeakPtr();

 private:
  // The profile this service belongs to.
  raw_ptr<ProfileIOS> profile_ = nullptr;

  // Whether a session is currently active.
  // Note: In subsequent CLs, this will be represented by ownership of a
  // `TTCSessionController`.
  bool is_session_active_ = false;

  // List of callbacks notified when `TTCServiceState` changes.
  base::RepeatingCallbackList<void(TTCServiceState)> state_changed_callbacks_;

  base::WeakPtrFactory<TTCKeyedService> weak_ptr_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TTC_KEYED_SERVICE_H_
