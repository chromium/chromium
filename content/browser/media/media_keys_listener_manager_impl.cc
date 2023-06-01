// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_keys_listener_manager_impl.h"

#include <memory>
#include <utility>

#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/active_media_session_controller.h"
#include "content/browser/media/system_media_controls_notifier.h"
#include "media/audio/audio_manager.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/idle/idle.h"

namespace content {

MediaKeysListenerManagerImpl::ListeningData::ListeningData() = default;

MediaKeysListenerManagerImpl::ListeningData::~ListeningData() = default;

// static
MediaKeysListenerManager* MediaKeysListenerManager::GetInstance() {
  if (!BrowserMainLoop::GetInstance())
    return nullptr;

  return BrowserMainLoop::GetInstance()->media_keys_listener_manager();
}

MediaKeysListenerManagerImpl::MediaKeysListenerManagerImpl()
    : active_media_session_controller_(
          std::make_unique<ActiveMediaSessionController>()) {
  DCHECK(!MediaKeysListenerManager::GetInstance());
}

MediaKeysListenerManagerImpl::~MediaKeysListenerManagerImpl() = default;

bool MediaKeysListenerManagerImpl::StartWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  StartListeningForMediaKeysIfNecessary();

  // We don't want to start watching the key for the
  // ActiveMediaSessionController if the ActiveMediaSessionController won't
  // receive events.
  bool is_active_media_session_controller =
      delegate == active_media_session_controller_.get();
  bool should_start_watching = !is_active_media_session_controller ||
                               CanActiveMediaSessionControllerReceiveEvents();

  // Tell the underlying MediaKeysListener to listen for the key.
  if (should_start_watching && media_keys_listener_ &&
      !media_keys_listener_->StartWatchingMediaKey(key_code)) {
    return false;
  }

  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  // If this is the ActiveMediaSessionController, just update the flag.
  if (is_active_media_session_controller) {
    listening_data->active_media_session_controller_listening = true;
    UpdateWhichKeysAreListenedFor();
    return true;
  }

  // Add the delegate to the list of listening delegates if necessary.
  if (!listening_data->listeners.HasObserver(delegate))
    listening_data->listeners.AddObserver(delegate);

  // Update listeners, as some ActiveMediaSessionController listeners may no
  // longer be needed.
  UpdateWhichKeysAreListenedFor();

  return true;
}

void MediaKeysListenerManagerImpl::StopWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  StartListeningForMediaKeysIfNecessary();

  // Find or create the list of listening delegates for this key code.
  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  // Update the listening data to remove this delegate.
  if (delegate == active_media_session_controller_.get())
    listening_data->active_media_session_controller_listening = false;
  else
    listening_data->listeners.RemoveObserver(delegate);

  UpdateWhichKeysAreListenedFor();
}

void MediaKeysListenerManagerImpl::DisableInternalMediaKeyHandling() {
  media_key_handling_enabled_ = false;
  UpdateWhichKeysAreListenedFor();
}

void MediaKeysListenerManagerImpl::EnableInternalMediaKeyHandling() {
  media_key_handling_enabled_ = true;
  UpdateWhichKeysAreListenedFor();
}

void MediaKeysListenerManagerImpl::OnMediaKeysAccelerator(
    const ui::Accelerator& accelerator) {
  // We should never receive an accelerator that was never registered.
  DCHECK(delegate_map_.contains(accelerator.key_code()));

#if BUILDFLAG(IS_APPLE)
  // For privacy, we don't want to handle media keys when the system is locked.
  // On Windows and Apple platforms, this will happen unless we explicitly
  // prevent it.
  // TODO(steimel): Consider adding an idle monitor instead and disabling the
  // RemoteCommandCenter/SystemMediaTransportControls on lock so that other OS
  // apps can take control.
  if (ui::CheckIdleStateIsLocked())
    return;
#endif

  ListeningData* listening_data = delegate_map_[accelerator.key_code()].get();

  // If the ActiveMediaSessionController is listening and is allowed to listen,
  // notify it of the media key press.
  if (listening_data->active_media_session_controller_listening &&
      CanActiveMediaSessionControllerReceiveEvents()) {
    active_media_session_controller_->OnMediaKeysAccelerator(accelerator);
    return;
  }

  // Otherwise, notify delegates.
  for (auto& delegate : listening_data->listeners)
    delegate.OnMediaKeysAccelerator(accelerator);
}

void MediaKeysListenerManagerImpl::SetIsMediaPlaying(bool is_playing) {
  is_media_playing_ = is_playing;
}

void MediaKeysListenerManagerImpl::OnNext() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_NEXT_TRACK)) {
    active_media_session_controller_->OnNext();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_NEXT_TRACK);
}

void MediaKeysListenerManagerImpl::OnPrevious() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PREV_TRACK)) {
    active_media_session_controller_->OnPrevious();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_PREV_TRACK);
}

void MediaKeysListenerManagerImpl::OnPlay() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
    active_media_session_controller_->OnPlay();
    return;
  }
  if (!is_media_playing_)
    MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnPause() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
    active_media_session_controller_->OnPause();
    return;
  }
  if (is_media_playing_)
    MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnPlayPause() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
    active_media_session_controller_->OnPlayPause();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnStop() {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_STOP)) {
    active_media_session_controller_->OnStop();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_STOP);
}

void MediaKeysListenerManagerImpl::OnSeek(const base::TimeDelta& time) {
  if (!CanActiveMediaSessionControllerReceiveEvents())
    return;
  active_media_session_controller_->OnSeek(time);
}

void MediaKeysListenerManagerImpl::OnSeekTo(const base::TimeDelta& time) {
  if (!CanActiveMediaSessionControllerReceiveEvents())
    return;
  active_media_session_controller_->OnSeekTo(time);
}

void MediaKeysListenerManagerImpl::MaybeSendKeyCode(ui::KeyboardCode key_code) {
  if (!delegate_map_.contains(key_code))
    return;
  ui::Accelerator accelerator(key_code, /*modifiers=*/0);
  OnMediaKeysAccelerator(accelerator);
}

void MediaKeysListenerManagerImpl::EnsureAuxiliaryServices() {
  if (auxiliary_services_started_)
    return;

#if BUILDFLAG(IS_APPLE)
  // On Apple platforms, we need to initialize the idle monitor in order to
  // check if the system is locked.
  ui::InitIdleMonitor();
#endif  // BUILDFLAG(IS_APPLE)

  auxiliary_services_started_ = true;
}

void MediaKeysListenerManagerImpl::StartListeningForMediaKeysIfNecessary() {
  if (system_media_controls_ || media_keys_listener_)
    return;

// TODO(crbug.com/1052397): Revisit once build flag switch of lacros-chrome is
// complete.
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  system_media_controls_ = system_media_controls::SystemMediaControls::Create(
      media::AudioManager::GetGlobalAppName());
#endif

  if (system_media_controls_) {
    system_media_controls_->AddObserver(this);
    system_media_controls_notifier_ =
        std::make_unique<SystemMediaControlsNotifier>(
            system_media_controls_.get());
  } else {
    // If we can't access system media controls, then directly listen for media
    // key keypresses instead.
    media_keys_listener_ = ui::MediaKeysListener::Create(
        this, ui::MediaKeysListener::Scope::kGlobal);
    DCHECK(media_keys_listener_);
  }

  EnsureAuxiliaryServices();
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

void MediaKeysListenerManagerImpl::UpdateWhichKeysAreListenedFor() {
  StartListeningForMediaKeysIfNecessary();

  if (system_media_controls_)
    UpdateSystemMediaControlsEnabledControls();
  else
    UpdateMediaKeysListener();
}

void MediaKeysListenerManagerImpl::UpdateSystemMediaControlsEnabledControls() {
  DCHECK(system_media_controls_);

  for (const auto& key_code_listening_data : delegate_map_) {
    const ui::KeyboardCode& key_code = key_code_listening_data.first;
    const ListeningData* listening_data = key_code_listening_data.second.get();

    bool should_enable = ShouldListenToKey(*listening_data);
    switch (key_code) {
      case ui::VKEY_MEDIA_PLAY_PAUSE:
        system_media_controls_->SetIsPlayPauseEnabled(should_enable);
        break;
      case ui::VKEY_MEDIA_NEXT_TRACK:
        system_media_controls_->SetIsNextEnabled(should_enable);
        break;
      case ui::VKEY_MEDIA_PREV_TRACK:
        system_media_controls_->SetIsPreviousEnabled(should_enable);
        break;
      case ui::VKEY_MEDIA_STOP:
        system_media_controls_->SetIsStopEnabled(should_enable);
        break;
      default:
        NOTREACHED();
    }
  }
}

void MediaKeysListenerManagerImpl::UpdateMediaKeysListener() {
  DCHECK(media_keys_listener_);

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
  return !listening_data.listeners.empty() ||
         (listening_data.active_media_session_controller_listening &&
          CanActiveMediaSessionControllerReceiveEvents());
}

bool MediaKeysListenerManagerImpl::AnyDelegatesListening() const {
  for (const auto& key_code_listening_data : delegate_map_) {
    if (!key_code_listening_data.second->listeners.empty())
      return true;
  }
  return false;
}

bool MediaKeysListenerManagerImpl::
    CanActiveMediaSessionControllerReceiveEvents() const {
  return media_key_handling_enabled_ && !AnyDelegatesListening();
}

bool MediaKeysListenerManagerImpl::ShouldActiveMediaSessionControllerReceiveKey(
    ui::KeyboardCode key_code) const {
  if (!CanActiveMediaSessionControllerReceiveEvents())
    return false;

  auto itr = delegate_map_.find(key_code);

  if (itr == delegate_map_.end())
    return false;

  ListeningData* listening_data = itr->second.get();

  DCHECK_NE(nullptr, listening_data);

  return listening_data->active_media_session_controller_listening;
}

}  // namespace content
