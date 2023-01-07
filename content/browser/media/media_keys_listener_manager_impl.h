// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace system_media_controls {
class SystemMediaControls;
}  // namespace system_media_controls

namespace content {

class ActiveMediaSessionController;
class SystemMediaControlsNotifier;

// Listens for media keys and decides which listeners receive which events. In
// particular, it owns one of its delegates (ActiveMediaSessionController), and
// only propagates to the ActiveMediaSessionController if no other delegates are
// listening to a particular media key.
class MediaKeysListenerManagerImpl
    : public MediaKeysListenerManager,
      public ui::MediaKeysListener::Delegate,
      public system_media_controls::SystemMediaControlsObserver {
 public:
  MediaKeysListenerManagerImpl();
  MediaKeysListenerManagerImpl(const MediaKeysListenerManagerImpl&) = delete;
  MediaKeysListenerManagerImpl& operator=(const MediaKeysListenerManagerImpl&) =
      delete;
  ~MediaKeysListenerManagerImpl() override;

  // MediaKeysListenerManager implementation.
  bool StartWatchingMediaKey(
      ui::KeyboardCode key_code,
      ui::MediaKeysListener::Delegate* delegate) override;
  void StopWatchingMediaKey(ui::KeyboardCode key_code,
                            ui::MediaKeysListener::Delegate* delegate) override;
  void DisableInternalMediaKeyHandling() override;
  void EnableInternalMediaKeyHandling() override;

  // ui::MediaKeysListener::Delegate:
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // system_media_controls::SystemMediaControlsObserver:
  void OnServiceReady() override {}
  void OnNext() override;
  void OnPrevious() override;
  void OnPlay() override;
  void OnPause() override;
  void OnPlayPause() override;
  void OnStop() override;
  void OnSeek(const base::TimeDelta& time) override;
  void OnSeekTo(const base::TimeDelta& time) override;

  // Informs the MediaKeysListener whether or not media is playing.
  void SetIsMediaPlaying(bool is_playing);

  ActiveMediaSessionController* active_media_session_controller_for_testing() {
    return active_media_session_controller_.get();
  }
  void SetMediaKeysListenerForTesting(
      std::unique_ptr<ui::MediaKeysListener> media_keys_listener) {
    media_keys_listener_ = std::move(media_keys_listener);
  }

 private:
  // ListeningData tracks which delegates are listening to a particular key. We
  // track the ActiveMediaSessionController separately from the other listeners
  // as it is treated differently.
  struct ListeningData {
    ListeningData();

    ListeningData(const ListeningData&) = delete;
    ListeningData& operator=(const ListeningData&) = delete;

    ~ListeningData();

    // True if the ActiveMediaSessionController is listening for this key.
    bool active_media_session_controller_listening = false;

    // Contains non-ActiveMediaSessionController listeners.
    base::ObserverList<ui::MediaKeysListener::Delegate> listeners;
  };

  void MaybeSendKeyCode(ui::KeyboardCode key_code);

  // Creates/Starts any OS-specific services needed for listening to media keys.
  void EnsureAuxiliaryServices();

  // Creates the SystemMediaControls or MediaKeysListener instance for listening
  // for media control events.
  void StartListeningForMediaKeysIfNecessary();

  ListeningData* GetOrCreateListeningData(ui::KeyboardCode key_code);

  // Starts/stops watching media keys based on the current state.
  void UpdateWhichKeysAreListenedFor();

  // Enables different controls on the system media controls based on current
  // state.
  void UpdateSystemMediaControlsEnabledControls();

  void UpdateMediaKeysListener();

  // True if we should listen for a key with the given listening data.
  bool ShouldListenToKey(const ListeningData& listening_data) const;

  // True if any delegates besides the ActiveMediaSessionController are
  // listening to any media keys.
  bool AnyDelegatesListening() const;

  // True if the ActiveMediaSessionController is allowed to receive events.
  bool CanActiveMediaSessionControllerReceiveEvents() const;

  // True if the ActiveMediaSessionController is allowed to receive events and
  // is listening for the event for the given key.
  bool ShouldActiveMediaSessionControllerReceiveKey(
      ui::KeyboardCode key_code) const;

  base::flat_map<ui::KeyboardCode, std::unique_ptr<ListeningData>>
      delegate_map_;
  std::unique_ptr<system_media_controls::SystemMediaControls>
      system_media_controls_;
  std::unique_ptr<ui::MediaKeysListener> media_keys_listener_;
  std::unique_ptr<ActiveMediaSessionController>
      active_media_session_controller_;
  std::unique_ptr<SystemMediaControlsNotifier> system_media_controls_notifier_;

  // False if media key handling has been explicitly disabled by a call to
  // |DisableInternalMediaKeyHandling()|.
  bool media_key_handling_enabled_ = true;

  // True if auxiliary services have already been started.
  bool auxiliary_services_started_ = false;

  bool is_media_playing_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
