// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_keys_listener_manager_impl.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/active_media_session_controller.h"
#include "content/browser/media/system_media_controls_notifier.h"
#include "content/browser/media/web_app_system_media_controls.h"
#include "content/public/common/content_features.h"
#include "media/audio/audio_manager.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/idle/idle.h"

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
#include "content/browser/media/web_app_system_media_controls_manager.h"
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

namespace content {

MediaKeysListenerManagerImpl::ListeningData::ListeningData() = default;

MediaKeysListenerManagerImpl::ListeningData::~ListeningData() = default;

// static
MediaKeysListenerManager* MediaKeysListenerManager::GetInstance() {
  if (!BrowserMainLoop::GetInstance())
    return nullptr;

  return BrowserMainLoop::GetInstance()->media_keys_listener_manager();
}

MediaKeysListenerManagerImpl::MediaKeysListenerManagerImpl() {
  DCHECK(!MediaKeysListenerManager::GetInstance());

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // If instanced system media controls are enabled, create the
  // web_app_system_media_controls_manager_ that handles web app related system
  // media controls.
  if (ShouldUseWebAppSystemMediaControls()) {
    web_app_system_media_controls_manager_ =
        std::make_unique<WebAppSystemMediaControlsManager>();
    web_app_system_media_controls_manager_->Init();
  }
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // Create the single ActiveMediaSessionController that follows the active
  // session. It can be unsupported due to feature flag being off or platform
  // constraints.
  // When instanced system media controls are enabled, this AMSC follows only
  // browser related sessions while web app related ones are handled by
  // web_app_system_media_controls_manager_.
  browser_active_media_session_controller_ =
      std::make_unique<ActiveMediaSessionController>(
          base::UnguessableToken::Null());
}

MediaKeysListenerManagerImpl::~MediaKeysListenerManagerImpl() = default;

bool MediaKeysListenerManagerImpl::StartWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate,
    base::UnguessableToken web_app_request_id) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  StartListeningForMediaKeysIfNecessary();

  // We don't want to start watching the key for an
  // ActiveMediaSessionController if an ActiveMediaSessionController won't
  // receive events.
  const bool is_delegate_for_browser =
      delegate == browser_active_media_session_controller_.get();
  const bool is_delegate_for_pwa = ShouldUseWebAppSystemMediaControls() &&
                                   IsDelegateForWebAppSession(delegate);

  const bool is_delegate_an_active_media_session_controller =
      is_delegate_for_browser || is_delegate_for_pwa;
  const bool should_start_watching =
      !is_delegate_an_active_media_session_controller ||
      CanActiveMediaSessionControllerReceiveEvents();

  // Tell the underlying MediaKeysListener to listen for the key.
  if (should_start_watching && media_keys_listener_ &&
      !media_keys_listener_->StartWatchingMediaKey(key_code)) {
    return false;
  }

  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  // If this is the ActiveMediaSessionController, just update the flag.
  if (is_delegate_an_active_media_session_controller) {
    // |delegate| should never be for both the browser and a PWA
    DCHECK(is_delegate_for_browser != is_delegate_for_pwa);

    if (is_delegate_for_browser) {
      listening_data->browser_active_media_session_controller_listening = true;
    } else if (is_delegate_for_pwa) {
      // If token is specified, it's a PWA that's starting to watch for a media
      // key. As a result, add it to the PWA list.
      DCHECK(web_app_request_id != base::UnguessableToken::Null());
      listening_data->listening_web_apps.insert(web_app_request_id);
    }
    UpdateWhichKeysAreListenedFor();

    // Notify test observers if they exist.
    if (test_observer_) {
      test_observer_->OnStartWatchingMediaKey(is_delegate_for_pwa);
    }
    return true;
  }

  // Add the delegate to the list of listening delegates if necessary.
  if (!listening_data->listeners.HasObserver(delegate))
    listening_data->listeners.AddObserver(delegate);

  // Update listeners, as some ActiveMediaSessionController listeners may no
  // longer be needed.
  UpdateWhichKeysAreListenedFor();

  // Notify test observers if they exist.
  if (test_observer_) {
    test_observer_->OnStartWatchingMediaKey(is_delegate_for_pwa);
  }

  return true;
}

void MediaKeysListenerManagerImpl::StopWatchingMediaKey(
    ui::KeyboardCode key_code,
    ui::MediaKeysListener::Delegate* delegate,
    base::UnguessableToken web_app_request_id) {
  DCHECK(ui::MediaKeysListener::IsMediaKeycode(key_code));
  DCHECK(delegate);
  StartListeningForMediaKeysIfNecessary();

  // Find or create the list of listening delegates for this key code.
  ListeningData* listening_data = GetOrCreateListeningData(key_code);

  if (delegate == browser_active_media_session_controller_.get()) {
    // Update the browser's listening data to remove this delegate.
    listening_data->browser_active_media_session_controller_listening = false;
  } else if (ShouldUseWebAppSystemMediaControls() &&
             IsDelegateForWebAppSession(delegate)) {
    // Remove this pwa_request_id from the listening data.
    listening_data->listening_web_apps.erase(web_app_request_id);
  } else {
    listening_data->listeners.RemoveObserver(delegate);
  }

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
  if (listening_data->browser_active_media_session_controller_listening &&
      CanActiveMediaSessionControllerReceiveEvents()) {
    browser_active_media_session_controller_->OnMediaKeysAccelerator(
        accelerator);
    return;
  }

  // Otherwise, notify delegates.
  for (auto& delegate : listening_data->listeners)
    delegate.OnMediaKeysAccelerator(accelerator);
}

void MediaKeysListenerManagerImpl::SetIsMediaPlaying(bool is_playing) {
  is_media_playing_ = is_playing;
}

void MediaKeysListenerManagerImpl::OnNext(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_NEXT_TRACK)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcNext,
                                 sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnNext();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_NEXT_TRACK);
}

void MediaKeysListenerManagerImpl::OnPrevious(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PREV_TRACK)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(
        WebAppSystemMediaControlsEvent::kPwaSmcPrevious, sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnPrevious();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_PREV_TRACK);
}

void MediaKeysListenerManagerImpl::OnPlay(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcPlay,
                                 sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnPlay();
    return;
  }
  if (!is_media_playing_)
    MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnPause(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcPause,
                                 sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnPause();
    return;
  }
  if (is_media_playing_)
    MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnPlayPause(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_PLAY_PAUSE)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(
        WebAppSystemMediaControlsEvent::kPwaSmcPlayPause, sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnPlayPause();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_PLAY_PAUSE);
}

void MediaKeysListenerManagerImpl::OnStop(
    system_media_controls::SystemMediaControls* sender) {
  if (ShouldActiveMediaSessionControllerReceiveKey(ui::VKEY_MEDIA_STOP)) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcStop,
                                 sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
    GetControllerForSystemMediaControls(sender)->OnStop();
    return;
  }
  MaybeSendKeyCode(ui::VKEY_MEDIA_STOP);
}

void MediaKeysListenerManagerImpl::OnSeek(
    system_media_controls::SystemMediaControls* sender,
    const base::TimeDelta& time) {
  if (!CanActiveMediaSessionControllerReceiveEvents())
    return;
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcSeek,
                               sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  GetControllerForSystemMediaControls(sender)->OnSeek(time);
}

void MediaKeysListenerManagerImpl::OnSeekTo(
    system_media_controls::SystemMediaControls* sender,
    const base::TimeDelta& time) {
  if (!CanActiveMediaSessionControllerReceiveEvents())
    return;
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  MaybeSendWebAppControlsEvent(WebAppSystemMediaControlsEvent::kPwaSmcSeekTo,
                               sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  GetControllerForSystemMediaControls(sender)->OnSeekTo(time);
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
  if (browser_system_media_controls_ || media_keys_listener_) {
    return;
  }

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || BUILDFLAG(IS_WIN)
  // Create SystemMediaControls with the SingletonHwnd.
  browser_system_media_controls_ =
      system_media_controls::SystemMediaControls::Create(
          media::AudioManager::GetGlobalAppName());
#elif BUILDFLAG(IS_MAC)
  browser_system_media_controls_ =
      system_media_controls::SystemMediaControls::Create(
          /*application_host=*/nullptr);
  if (on_system_media_controls_bridge_created_callback_for_testing_) {
    browser_system_media_controls_->SetOnBridgeCreatedCallbackForTesting(
        std::move(
            on_system_media_controls_bridge_created_callback_for_testing_));
  }
#endif

  if (browser_system_media_controls_) {
    browser_system_media_controls_->AddObserver(this);
    // Pass Null request ID so this notifier will track the active session, not
    // a specific session.
    browser_system_media_controls_notifier_ =
        std::make_unique<SystemMediaControlsNotifier>(
            browser_system_media_controls_.get(),
            base::UnguessableToken::Null());
  } else {
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

  if (browser_system_media_controls_) {
    UpdateSystemMediaControlsEnabledControls();
  } else {
    UpdateMediaKeysListener();
  }
}

void MediaKeysListenerManagerImpl::UpdateSystemMediaControlsEnabledControls() {
  // This should be safe to call even if nothing is playing - which should
  // result in a no-op.

  if (browser_system_media_controls_) {
    // Update the browser box.
    for (const auto& key_code_listening_data : delegate_map_) {
      const ui::KeyboardCode& key_code = key_code_listening_data.first;
      const ListeningData* listening_data =
          key_code_listening_data.second.get();

      bool should_enable = ShouldListenToKey(*listening_data);
      switch (key_code) {
        case ui::VKEY_MEDIA_PLAY_PAUSE:
          browser_system_media_controls_->SetIsPlayPauseEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_NEXT_TRACK:
          browser_system_media_controls_->SetIsNextEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_PREV_TRACK:
          browser_system_media_controls_->SetIsPreviousEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_STOP:
          browser_system_media_controls_->SetIsStopEnabled(should_enable);
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // This loops over active web app instanced system media controls and updates
  // what controls are available on each set of controls.
  if (!ShouldUseWebAppSystemMediaControls()) {
    return;
  }

  for (auto* controls :
       web_app_system_media_controls_manager_->GetAllControls()) {
    system_media_controls::SystemMediaControls* smc =
        controls->GetSystemMediaControls();
    base::UnguessableToken request_id = controls->GetRequestID();

    for (const auto& key_code_listening_data : delegate_map_) {
      const ui::KeyboardCode& key_code = key_code_listening_data.first;
      const ListeningData* listening_data =
          key_code_listening_data.second.get();

      // If we don't see this token in the listening_pwas, we should not
      // enable it. If we do find the token in the listening_pwas, we will
      // enable it.
      bool should_enable =
          listening_data->listening_web_apps.contains(request_id);
      switch (key_code) {
        case ui::VKEY_MEDIA_PLAY_PAUSE:
          smc->SetIsPlayPauseEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_NEXT_TRACK:
          smc->SetIsNextEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_PREV_TRACK:
          smc->SetIsPreviousEnabled(should_enable);
          break;
        case ui::VKEY_MEDIA_STOP:
          smc->SetIsStopEnabled(should_enable);
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }
    }
  }
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
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
  // TODO(crbug.com/40943399) verify if this needs a PWA check.
  return !listening_data.listeners.empty() ||
         (listening_data.browser_active_media_session_controller_listening &&
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

  return listening_data->browser_active_media_session_controller_listening ||
         (ShouldUseWebAppSystemMediaControls() &&
          !listening_data->listening_web_apps.empty());
}

bool MediaKeysListenerManagerImpl::ShouldUseWebAppSystemMediaControls() const {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // This feature is enabled by default on Windows, disabled on mac.
  return base::FeatureList::IsEnabled(features::kWebAppSystemMediaControls);
#else
  return false;
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
}

bool MediaKeysListenerManagerImpl::IsDelegateForWebAppSession(
    ui::MediaKeysListener::Delegate* delegate) {
#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  std::vector<WebAppSystemMediaControls*> pwa_controls =
      web_app_system_media_controls_manager_->GetAllControls();

  for (auto* curr_controls : pwa_controls) {
    if (curr_controls->GetController() == delegate) {
      return true;
    }
  }
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  return false;
}

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
void MediaKeysListenerManagerImpl::MaybeSendWebAppControlsEvent(
    WebAppSystemMediaControlsEvent event,
    system_media_controls::SystemMediaControls* sender) {
  if (web_app_system_media_controls_manager_ &&
      web_app_system_media_controls_manager_
          ->GetWebAppSystemMediaControlsForSystemMediaControls(sender)) {
    // Since the sender is registered with the
    // web_app_system_media_controls_manager we're good to go ahead and fire the
    // histogram for instanced pwa controls.
    base::UmaHistogramEnumeration("WebApp.Media.SystemMediaControls", event);
  }
}
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

ActiveMediaSessionController*
MediaKeysListenerManagerImpl::GetControllerForSystemMediaControls(
    system_media_controls::SystemMediaControls* system_media_controls) {
  // Check if system_media_controls is browser box.
  // If kWebAppSystemMediaControls is not supported, we should always use the
  // browser controller.
  if (!ShouldUseWebAppSystemMediaControls() ||
      system_media_controls == browser_system_media_controls_.get()) {
    return browser_active_media_session_controller_.get();
  }

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // Ask the manager for the appropriate ActiveMediaSessionController.
  WebAppSystemMediaControls* controls =
      web_app_system_media_controls_manager_
          ->GetWebAppSystemMediaControlsForSystemMediaControls(
              system_media_controls);
  if (controls) {
    return controls->GetController();
  }
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

  // It's unexpected that any code asks for the controller for a
  // system_media_controls object we don't know about.
  NOTREACHED();
}

}  // namespace content
