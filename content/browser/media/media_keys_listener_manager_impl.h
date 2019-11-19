// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace service_manager {
class Connector;
}  // namespace service_manager

namespace content {

class HardwareKeyMediaController;
class SystemMediaControlsNotifier;

// Listens for media keys and decides which listeners receive which events. In
// particular, it owns one of its delegates (HardwareKeyMediaController), and
// only propagates to the HardwareKeyMediaController if no other delegates are
// listening to a particular media key.
class CONTENT_EXPORT MediaKeysListenerManagerImpl
    : public MediaKeysListenerManager,
      public ui::MediaKeysListener::Delegate {
 public:
  explicit MediaKeysListenerManagerImpl(service_manager::Connector* connector);
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

  // Informs the MediaKeysListener whether or not media is playing.
  // TODO(https://crbug.com/974035): Once the MediaKeysListenerManager has been
  // refactored to work with system media controls this should no longer be
  // needed and should be deleted.
  void SetIsMediaPlaying(bool is_playing);

  HardwareKeyMediaController* hardware_key_media_controller_for_testing() {
    return hardware_key_media_controller_.get();
  }
  void SetMediaKeysListenerForTesting(
      std::unique_ptr<ui::MediaKeysListener> media_keys_listener) {
    media_keys_listener_ = std::move(media_keys_listener);
  }

 private:
  // ListeningData tracks which delegates are listening to a particular key. We
  // track the HardwareKeyMediaController separately from the other listeners as
  // it is treated differently.
  struct ListeningData {
    ListeningData();
    ~ListeningData();

    // True if the HardwareKeyMediaController is listening for this key.
    bool hardware_key_media_controller_listening;

    // Contains non-HardwareKeyMediaController listeners.
    base::ObserverList<ui::MediaKeysListener::Delegate> listeners;

   private:
    DISALLOW_COPY_AND_ASSIGN(ListeningData);
  };

  // Creates/Starts any OS-specific services needed for listening to media keys.
  void EnsureAuxiliaryServices();

  void EnsureMediaKeysListener();
  ListeningData* GetOrCreateListeningData(ui::KeyboardCode key_code);

  // Starts/stops watching media keys based on the current state.
  void UpdateKeyListening();

  // True if we should listen for a key with the given listening data.
  bool ShouldListenToKey(const ListeningData& listening_data) const;

  // True if any delegates besides the HardwareKeyMediaController are listening
  // to any media keys.
  bool AnyDelegatesListening() const;

  // True if the HardwareKeyMediaController is allowed to receive events.
  bool CanHardwareKeyMediaControllerReceiveEvents() const;

  base::flat_map<ui::KeyboardCode, std::unique_ptr<ListeningData>>
      delegate_map_;
  std::unique_ptr<ui::MediaKeysListener> media_keys_listener_;
  service_manager::Connector* connector_;
  std::unique_ptr<HardwareKeyMediaController> hardware_key_media_controller_;
  std::unique_ptr<SystemMediaControlsNotifier> system_media_controls_notifier_;

  // False if media key handling has been explicitly disabled by a call to
  // |DisableInternalMediaKeyHandling()|.
  bool media_key_handling_enabled_;

  // True if auxiliary services have already been started.
  bool auxiliary_services_started_;

  bool is_media_playing_ = false;

  DISALLOW_COPY_AND_ASSIGN(MediaKeysListenerManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
