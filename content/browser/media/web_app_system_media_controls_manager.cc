// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/web_app_system_media_controls_manager.h"

#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "components/system_media_controls/system_media_controls.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/media/media_keys_listener_manager_impl.h"
#include "content/browser/media/system_media_controls_notifier.h"
#include "content/browser/media/web_app_system_media_controls.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "media/audio/audio_manager.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "ui/gfx/native_widget_types.h"

#if BUILDFLAG(IS_MAC)
#include "components/remote_cocoa/browser/application_host.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

#if BUILDFLAG(IS_MAC)
remote_cocoa::ApplicationHost* GetApplicationHostFromWebContents(
    content::WebContents* web_contents) {
  // Get the ApplicationHost (ie. the browser-side component corresponding to
  // the NSApplication running in an app shim process) for the web contents.
  return remote_cocoa::ApplicationHost::GetForNativeView(
      web_contents ? web_contents->GetNativeView() : gfx::NativeView());
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
intptr_t GetHWNDFromWebContents(content::WebContents* web_contents) {
  // Get the HWND for the window containing the web contents (Recreation
  // of HWNDForNativeView).
  gfx::NativeView native_view = web_contents->GetNativeView();
  if (native_view && native_view->GetRootWindow()) {
    return reinterpret_cast<intptr_t>(
        native_view->GetHost()->GetAcceleratedWidget());
  }
  return -1;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

namespace content {

WebAppSystemMediaControlsManager::WebAppSystemMediaControlsManager() = default;

WebAppSystemMediaControlsManager::~WebAppSystemMediaControlsManager() = default;

void WebAppSystemMediaControlsManager::Init() {
  CHECK(initialized_ == false);
  initialized_ = true;
  CHECK(!audio_focus_manager_.is_bound());
  CHECK(!audio_focus_observer_receiver_.is_bound());
  if (skip_mojo_connection_for_testing_) {
    return;
  }
  TryConnectToAudioFocusManager();
}

void WebAppSystemMediaControlsManager::TryConnectToAudioFocusManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CHECK(!audio_focus_manager_.is_bound());

  // Bind our remote AudioFocusManager endpoint
  GetMediaSessionService().BindAudioFocusManager(
      audio_focus_manager_.BindNewPipeAndPassReceiver());
  // Set error handler
  audio_focus_manager_.set_disconnect_handler(base::BindOnce(
      &WebAppSystemMediaControlsManager::OnMojoError, base::Unretained(this)));

  CHECK(!audio_focus_observer_receiver_.is_bound());
  // Bind our receiver AudioFocusObserver endpoint and register it as an
  // observer of our remote AudioFocusManager endpoint bound above.
  audio_focus_manager_->AddObserver(
      audio_focus_observer_receiver_.BindNewPipeAndPassRemote());

  audio_focus_observer_receiver_.set_disconnect_handler(base::BindOnce(
      &WebAppSystemMediaControlsManager::OnMojoError, base::Unretained(this)));
}

void WebAppSystemMediaControlsManager::OnMojoError() {
  audio_focus_manager_.reset();
  audio_focus_observer_receiver_.reset();
}

// AudioFocusObserver overrides
void WebAppSystemMediaControlsManager::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr state) {
  CHECK(initialized_);
  const std::optional<base::UnguessableToken>& maybe_id = state->request_id;
  if (!maybe_id.has_value()) {
    return;
  }

  base::UnguessableToken request_id = maybe_id.value();

  // Get the web contents associated with the request_id
  content::WebContents* web_contents =
      MediaSession::GetWebContentsFromRequestId(request_id);

  WebAppSystemMediaControls* existing_controls = nullptr;

  // It's possible no web contents is returned if the web contents
  // has been destroyed.
  if (!web_contents) {
    return;
  }

  // Check if the web contents found is in a dPWA. Occasionally, we've found it
  // is possible that the web contents does not have a delegate - we should just
  // abort in that scenario.
  WebContentsDelegate* web_contents_delegate = web_contents->GetDelegate();
  if (!web_contents_delegate) {
    return;
  }

  bool is_web_contents_for_web_app =
      web_contents_delegate->ShouldUseInstancedSystemMediaControls() ||
      always_assume_web_app_for_testing_;
  if (!is_web_contents_for_web_app) {
    // Non-webapp updates are handled by media_keys_listener_manager_impl and do
    // not need any intervention from us here.
    return;
  }

  // At this point, we know this web contents is for a dPWA.
  // See if controls already exists for this request id.
  existing_controls = GetControlsForRequestId(request_id);

  // It's also the right time to fire telemetry that a PWA session is playing
  // audio since we know it's not a browser.
  base::UmaHistogramEnumeration(
      "WebApp.Media.SystemMediaControls",
      WebAppSystemMediaControlsEvent::kPwaPlayingMedia);

  // if the controls don't exist, we need to make an SMC and the
  // controls object.
  if (!existing_controls) {
#if BUILDFLAG(IS_WIN)
    // `window` is -1 if no HWND found.
    intptr_t window = GetHWNDFromWebContents(web_contents);
    std::unique_ptr<system_media_controls::SystemMediaControls>
        system_media_controls =
            system_media_controls::SystemMediaControls::Create(
                media::AudioManager::GetGlobalAppName(), window);
#else
    remote_cocoa::ApplicationHost* application_host =
        GetApplicationHostFromWebContents(web_contents);

    std::unique_ptr<system_media_controls::SystemMediaControls>
        system_media_controls =
            system_media_controls::SystemMediaControls::Create(
                application_host);

    if (on_system_media_controls_bridge_created_callback_for_testing_) {
      system_media_controls->SetOnBridgeCreatedCallbackForTesting(
          on_system_media_controls_bridge_created_callback_for_testing_);
    }
#endif  // BUILDFLAG(IS_WIN)

    if (!system_media_controls) {
      return;
    }

    // The global MKLM should be set as an observer for the newly created
    // SystemMediaControls object.
    system_media_controls->AddObserver(
        BrowserMainLoop::GetInstance()->media_keys_listener_manager());

    auto notifier = std::make_unique<SystemMediaControlsNotifier>(
        system_media_controls.get(), request_id);
    auto controller =
        std::make_unique<ActiveMediaSessionController>(request_id);

    controls_map_.emplace(request_id,
                          std::make_unique<WebAppSystemMediaControls>(
                              request_id, std::move(system_media_controls),
                              std::move(notifier), std::move(controller)));

    if (test_observer_) {
      test_observer_->OnWebAppAdded(request_id);
    }
  } else {
    // If the requestID already exists in the map, we still need to rebind
    // the notifier and controller as they have been invalidated.
    // We observe this behavior for example, when triggering 'next track'
    // which causes onFocusGained to fire but the notifier and controller
    // cannot be reused afterwards.
    existing_controls->SetNotifier(
        std::make_unique<SystemMediaControlsNotifier>(
            existing_controls->GetSystemMediaControls(), request_id));
    existing_controls->SetController(
        std::make_unique<ActiveMediaSessionController>(request_id));
  }
}

void WebAppSystemMediaControlsManager::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr state) {
  CHECK(initialized_);

  if (!state->request_id) {
    return;
  }

  auto it = controls_map_.find(state->request_id.value());

  // There will be no entry if it was a browser session that lost focus.
  if (it == controls_map_.end()) {
    return;
  }

  // Tell the OS that audio stopped and to hide the UI.

  // For the browser, the SystemMediaControlsNotifier automatically follows the
  // "active" media session. However, because all PWA media controls are
  // associated with a specific media session, they don't receive the same
  // metadata updates. (crbug/326411160 for more information why).
  // Instead, `this` receives a FocusLost updates via AudioFocusObserver, and we
  // must then do the OS UI cleanup ourselves.

  // Because SystemMediaControlsNotifier keeps internal timers/logic to debounce
  // metadata updates, we leverage the existing logic there by directly calling
  // MediaSessionInfoChanged (a MediaControllerObserver function)
  // with empty information to force the SMCNotifier to take the normal cleanup
  // path to hide the OS UI and stop all running debounce timers. (Although
  // `state` has a session_info field, we can't use that because it will have
  // information. we need to pass an empty information so
  // MediaSessionInfoChanged will think a track ended and take the cleanup
  // route)
  content::SystemMediaControlsNotifier* notifier = it->second->GetNotifier();
  media_session::mojom::MediaSessionInfoPtr empty_info;
  notifier->MediaSessionInfoChanged(std::move(empty_info));
}

void WebAppSystemMediaControlsManager::OnRequestIdReleased(
    const base::UnguessableToken& request_id) {
  CHECK(initialized_);

  auto it = controls_map_.find(request_id);
  if (it == controls_map_.end()) {
    return;
  }

  // the controls_map_ holds unique_ptr so most of the destruction will happen
  // automatically.
  controls_map_.erase(request_id);
}

WebAppSystemMediaControls*
WebAppSystemMediaControlsManager::GetControlsForRequestId(
    base::UnguessableToken request_id) {
  CHECK(initialized_);
  auto it = controls_map_.find(request_id);
  if (it != controls_map_.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

WebAppSystemMediaControls* WebAppSystemMediaControlsManager::
    GetWebAppSystemMediaControlsForSystemMediaControls(
        system_media_controls::SystemMediaControls* system_media_controls) {
  CHECK(initialized_);
  for (auto& it : controls_map_) {
    WebAppSystemMediaControls* curr_controls = it.second.get();
    if (curr_controls->GetSystemMediaControls() == system_media_controls) {
      return curr_controls;
    }
  }
  return nullptr;
}

std::vector<WebAppSystemMediaControls*>
WebAppSystemMediaControlsManager::GetAllControls() {
  CHECK(initialized_);
  std::vector<WebAppSystemMediaControls*> vec;

  for (auto& it : controls_map_) {
    vec.push_back(it.second.get());
  }

  return vec;
}

void WebAppSystemMediaControlsManager::
    SetOnSystemMediaControlsBridgeCreatedCallbackForTesting(
        base::RepeatingCallback<void()> callback) {
  on_system_media_controls_bridge_created_callback_for_testing_ =
      std::move(callback);
}

}  // namespace content
