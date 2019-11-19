// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_keys_listener_manager_impl.h"

#include <memory>
#include <utility>

#include "build/build_config.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/hardware_key_media_controller.h"
#include "content/browser/media/system_media_controls_notifier.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/idle/idle.h"

namespace content {

MediaKeysListenerManagerImpl::ListeningData::ListeningData()
    : hardware_key_media_controller_listening(false) {}

MediaKeysListenerManagerImpl::ListeningData::~ListeningData() = default;

// static
MediaKeysListenerManager* MediaKeysListenerManager::GetInstance() {
  if (!BrowserMainLoop::GetInstance())
    return nullptr;

  return BrowserMainLoop::GetInstance()->media_keys_listener_manager();
}

MediaKeysListenerManagerImpl::MediaKeysListenerManagerImpl(
    service_manager::Connector* connector)
    : connector_(connector),
      hardware_key_media_controller_(
          std::make_unique<HardwareKeyMediaController>(connector_)),
      media_key_handling_enabled_(true),
      auxiliary_services_started_(false) {
  DCHECK(!MediaKeysListenerManager::GetInstance());
}

MediaKeysListenerManagerImpl::~MediaKeysListenerManagerImpl() = default;

bool MediaKeysListenerManagerImpl::StartWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  EnsureMediaKeysListener();

  // We don't want to start watching the key for the HardwareKeyMediaController
  // if the HardwareKeyMediaController won't receive events.
  bool is_hardware_key_media_controller =
      delegate == hardware_key_media_controller_.get();
  bool should_start_watching = !is_hardware_key_media_controller ||
                               CanHardwareKeyMediaControllerReceiveEvents();

  // Tell the underlying MediaKeysListener to listen for the key.
  if (should_start_watching &&
      !media_keys_listener_->StartWatchingMediaKey(key_code)) {
    return false;
  }

  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  // If this is the HardwareKeyMediaController, just update the flag.
  if (is_hardware_key_media_controller) {
    listening_data->hardware_key_media_controller_listening = true;
    return true;
  }

  // Add the delegate to the list of listening delegates if necessary.
  if (!listening_data->listeners.HasObserver(delegate))
    listening_data->listeners.AddObserver(delegate);

  // Update listeners, as some HardwareKeyMediaController listeners may no
  // longer be needed.
  UpdateKeyListening();

  return true;
}

void MediaKeysListenerManagerImpl::StopWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  EnsureMediaKeysListener();

  // Find or create the list of listening delegates for this key code.
  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  // Update the listening data to remove this delegate.
  if (delegate == hardware_key_media_controller_.get()) {
    listening_data->hardware_key_media_controller_listening = false;
    if (!ShouldListenToKey(*listening_data))
      media_keys_listener_->StopWatchingMediaKey(key_code);
  } else {
    listening_data->listeners.RemoveObserver(delegate);
    UpdateKeyListening();
  }
}

void MediaKeysListenerManagerImpl::DisableInternalMediaKeyHandling() {
  media_key_handling_enabled_ = false;
  UpdateKeyListening();
}

void MediaKeysListenerManagerImpl::EnableInternalMediaKeyHandling() {
  media_key_handling_enabled_ = true;
  UpdateKeyListening();
}

void MediaKeysListenerManagerImpl::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  // We should never receive an accelerator that was never registered.
  DCHECK(delegate_map_.contains(accelerator.key_code()));

#if defined(OS_MACOSX)
  // For privacy, we don't want to handle media keys when the system is locked.
  // On Windows and Mac OS X, this will happen unless we explicitly prevent it.
  // TODO(steimel): Consider adding an idle monitor instead and disabling the
  // RemoteCommandCenter/SystemMediaTransportControls on lock so that other OS
  // apps can take control.
  if (ui::CheckIdleStateIsLocked())
    return;
#endif

  ListeningData* listening_data = delegate_map_[accelerator.key_code()].get();

  // If the HardwareKeyMediaController is listening and is allowed to listen,
  // notify it of the media key press.
  if (listening_data->hardware_key_media_controller_listening &&
      CanHardwareKeyMediaControllerReceiveEvents()) {
    hardware_key_media_controller_->OnMediaKeysAccelerator(accelerator);
    return;
  }

  // Otherwise, notify delegates.
  for (auto& delegate : listening_data->listeners)
    delegate.OnMediaKeysAccelerator(accelerator);
}

void MediaKeysListenerManagerImpl::SetIsMediaPlaying(bool is_playing) {
  if (is_media_playing_ == is_playing)
    return;

  is_media_playing_ = is_playing;

  if (media_keys_listener_)
    media_keys_listener_->SetIsMediaPlaying(is_media_playing_);
}

void MediaKeysListenerManagerImpl::EnsureAuxiliaryServices() {
  if (auxiliary_services_started_)
    return;

  // Keep the SystemMediaControls notified of media playback state and metadata.
  system_media_controls::SystemMediaControls* system_media_controls =
      system_media_controls::SystemMediaControls::GetInstance();
  if (system_media_controls) {
    system_media_controls_notifier_ =
        std::make_unique<SystemMediaControlsNotifier>(connector_,
                                                      system_media_controls);
  }

#if defined(OS_MACOSX)
  // On Mac OS, we need to initialize the idle monitor in order to check if the
  // system is locked.
  ui::InitIdleMonitor();
#endif  // defined(OS_MACOSX)

  auxiliary_services_started_ = true;
}

void MediaKeysListenerManagerImpl::EnsureMediaKeysListener() {
  if (media_keys_listener_)
    return;

  EnsureAuxiliaryServices();

  media_keys_listener_ = ui::MediaKeysListener::Create(
      this, ui::MediaKeysListener::Scope::kGlobal);
  DCHECK(media_keys_listener_);

  media_keys_listener_->SetIsMediaPlaying(is_media_playing_);
}

MediaKeysListenerManagerImpl::ListeningData*
MediaKeysListenerManagerImpl::GetOrCreateListeningData(
    ui::KeyboardCode key_code) {
  auto listening_data_itr = delegate_map_.find(key_code);
  if (listening_data_itr == delegate_map_.end()) {
    listening_data_itr =
        delegate_map_
            .insert(std::make_pair(key_code, std::make_unique<ListeningData>()))
            .first;
  }
  return listening_data_itr->second.get();
}

void MediaKeysListenerManagerImpl::UpdateKeyListening() {
  EnsureMediaKeysListener();

  for (const auto& key_code_listening_data : delegate_map_) {
    const ui::KeyboardCode& key_code = key_code_listening_data.first;
    const ListeningData* listening_data = key_code_listening_data.second.get();

    if (ShouldListenToKey(*listening_data))
      media_keys_listener_->StartWatchingMediaKey(key_code);
    else
      media_keys_listener_->StopWatchingMediaKey(key_code);
  }
}

bool MediaKeysListenerManagerImpl::ShouldListenToKey(
    const ListeningData& listening_data) const {
  return listening_data.listeners.might_have_observers() ||
         (listening_data.hardware_key_media_controller_listening &&
          CanHardwareKeyMediaControllerReceiveEvents());
}

bool MediaKeysListenerManagerImpl::AnyDelegatesListening() const {
  for (const auto& key_code_listening_data : delegate_map_) {
    if (key_code_listening_data.second->listeners.might_have_observers())
      return true;
  }
  return false;
}

bool MediaKeysListenerManagerImpl::CanHardwareKeyMediaControllerReceiveEvents()
    const {
  return media_key_handling_enabled_ && !AnyDelegatesListening();
}

}  // namespace content
