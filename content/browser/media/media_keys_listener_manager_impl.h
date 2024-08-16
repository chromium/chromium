// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
#define CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_

#include <memory>
#include <set>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/system_media_controls/system_media_controls_observer.h"
#include "content/public/browser/media_keys_listener_manager.h"
#include "ui/base/accelerators/media_keys_listener.h"
#include "ui/events/keycodes/keyboard_codes.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS 1
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

namespace system_media_controls {
class SystemMediaControls;
}  // namespace system_media_controls

namespace content {

class ActiveMediaSessionController;
class SystemMediaControlsNotifier;

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
class WebAppSystemMediaControlsManager;
enum class WebAppSystemMediaControlsEvent;
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

class MediaKeysListenerManagerImplTestObserver {
 public:
  virtual void OnStartWatchingMediaKey(bool is_pwa) {}
};

// Listens for media keys and decides which listeners receive which events. In
// particular, it owns one of its delegates (ActiveMediaSessionController), and
// only propagates to the ActiveMediaSessionController if no other delegates are
// listening to a particular media key.

// It also owns the WebAppSystemMediaControlsManager which it retrieves
// information from and propagates to if no other delegates are listening to a
// particular media key. See WebAppSystemMediaControlsManager for more
// information on that class' responsibilities.
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
  bool StartWatchingMediaKey(ui::KeyboardCode key_code,
                             ui::MediaKeysListener::Delegate* delegate,
                             base::UnguessableToken web_app_request_id =
                                 base::UnguessableToken::Null()) override;
  void StopWatchingMediaKey(ui::KeyboardCode key_code,
                            ui::MediaKeysListener::Delegate* delegate,
                            base::UnguessableToken web_app_request_id =
                                base::UnguessableToken::Null()) override;
  void DisableInternalMediaKeyHandling() override;
  void EnableInternalMediaKeyHandling() override;

  // ui::MediaKeysListener::Delegate:
  void OnMediaKeysAccelerator(const ui::Accelerator& accelerator) override;

  // system_media_controls::SystemMediaControlsObserver:
  void OnServiceReady() override {}
  void OnNext(system_media_controls::SystemMediaControls* sender) override;
  void OnPrevious(system_media_controls::SystemMediaControls* sender) override;
  void OnPlay(system_media_controls::SystemMediaControls* sender) override;
  void OnPause(system_media_controls::SystemMediaControls* sender) override;
  void OnPlayPause(system_media_controls::SystemMediaControls* sender) override;
  void OnStop(system_media_controls::SystemMediaControls* sender) override;
  void OnSeek(system_media_controls::SystemMediaControls* sender,
              const base::TimeDelta& time) override;
  void OnSeekTo(system_media_controls::SystemMediaControls* sender,
                const base::TimeDelta& time) override;

  // Informs the MediaKeysListener whether or not media is playing.
  void SetIsMediaPlaying(bool is_playing);

  ActiveMediaSessionController* active_media_session_controller_for_testing() {
    return browser_active_media_session_controller_.get();
  }
  void SetMediaKeysListenerForTesting(
      std::unique_ptr<ui::MediaKeysListener> media_keys_listener) {
    media_keys_listener_ = std::move(media_keys_listener);
  }
#if BUILDFLAG(IS_MAC)
  WebAppSystemMediaControlsManager*
  web_app_system_media_controls_manager_for_testing() {
    return web_app_system_media_controls_manager_.get();
  }
  void SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
      base::RepeatingCallback<void()> callback) {
    on_system_media_controls_bridge_created_callback_for_testing_ =
        std::move(callback);
  }
#endif  // BUILDFLAG(IS_MAC)

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
    bool browser_active_media_session_controller_listening = false;

    // Contains non-ActiveMediaSessionController listeners.
    base::ObserverList<ui::MediaKeysListener::Delegate> listeners;

    // These request IDs represent dPWAs that are listening for this key.
    std::set<base::UnguessableToken> listening_web_apps;
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

  // Performs a platform and feature flag check and returns true if we should
  // use instanced system media controls for dPWAs.
  bool ShouldUseWebAppSystemMediaControls() const;

  // Returns true if |delegate| is an ActiveMediaSessionController for a dPWA.
  bool IsDelegateForWebAppSession(ui::MediaKeysListener::Delegate* delegate);

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // Given a SystemMediaControls |sender| and an |event|, if the |sender|
  // is a web app, will fire WebApp.Media.SystemMediaControls histogram with
  // the associated |event|. If the |sender| is the browser, this function
  // does nothing.
  void MaybeSendWebAppControlsEvent(
      WebAppSystemMediaControlsEvent event,
      system_media_controls::SystemMediaControls* sender);
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

  // Gets the ActiveMediaSessionController associated with |smc_sender|
  ActiveMediaSessionController* GetControllerForSystemMediaControls(
      system_media_controls::SystemMediaControls* system_media_controls);

  base::flat_map<ui::KeyboardCode, std::unique_ptr<ListeningData>>
      delegate_map_;
  std::unique_ptr<ui::MediaKeysListener> media_keys_listener_;

  // TODO(crbug.com/40943388) consider moving these somewhere else.
  // Browser's connection to the system media controls.
  std::unique_ptr<system_media_controls::SystemMediaControls>
      browser_system_media_controls_;
  std::unique_ptr<ActiveMediaSessionController>
      browser_active_media_session_controller_;
  std::unique_ptr<SystemMediaControlsNotifier>
      browser_system_media_controls_notifier_;

#if USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS
  // Owning reference to web apps' connections to the system media controls.
  // See WebAppSystemMediaControlsManager for this classes' responsibilities.
  std::unique_ptr<WebAppSystemMediaControlsManager>
      web_app_system_media_controls_manager_;
#endif  // USE_INSTANCED_SYSTEM_MEDIA_CONTROLS_FOR_WEB_APPS

  // False if media key handling has been explicitly disabled by a call to
  // |DisableInternalMediaKeyHandling()|.
  bool media_key_handling_enabled_ = true;

  // True if auxiliary services have already been started.
  bool auxiliary_services_started_ = false;

  bool is_media_playing_ = false;

  // Tests that friend this class will use this mechanism to be notified of
  // certain events that are otherwise difficult to wait for.
  raw_ptr<MediaKeysListenerManagerImplTestObserver> test_observer_ = nullptr;
  void SetObserverForTesting(
      MediaKeysListenerManagerImplTestObserver* observer) {
    test_observer_ = observer;
  }
#if BUILDFLAG(IS_MAC)
  base::RepeatingCallback<void()>
      on_system_media_controls_bridge_created_callback_for_testing_;
#endif  // BUILDFLAG(IS_MAC)

  friend class WebAppSystemMediaControlsBrowserTest;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_MEDIA_KEYS_LISTENER_MANAGER_IMPL_H_
