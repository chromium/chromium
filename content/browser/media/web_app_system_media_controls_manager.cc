// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/web_app_system_media_controls_manager.h"

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
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/native_widget_types.h"

namespace {

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
  const absl::optional<base::UnguessableToken>& maybe_id = state->request_id;
  if (!maybe_id.has_value()) {
    return;
  }

  base::UnguessableToken request_id = maybe_id.value();
  DVLOG(1) << "WebAppSystemMediaControlsManager::OnFocusGained, "
              "request id = "
           << request_id.ToString();

  // Get the web contents associated with the request_id
  content::WebContents* web_contents =
      MediaSession::GetWebContentsFromRequestId(request_id);

  WebAppSystemMediaControls* existing_controls = nullptr;

  // It's possible no web contents is returned if the web contents
  // has been destroyed.
  if (!web_contents) {
    DVLOG(1) << "WebAppSystemMediaControlsManager::OnFocusGained received "
                "destroyed web contents";
    return;
  }

  // Check if the web contents found is in a dPWA.
  bool is_web_contents_for_web_app =
      web_contents->GetDelegate()->ShouldUseInstancedSystemMediaControls() ||
      always_assume_web_app_for_testing_;
  if (!is_web_contents_for_web_app) {
    // TODO(crbug.com/1502981) this is the only place we have the request_id for
    // the browser's controls, but logically doesn't make a ton of sense to do
    // browser handling in here. This bug tracks investigating moving it
    // somewhere else.

    MediaKeysListenerManagerImpl* media_keys_listener_manager_impl =
        BrowserMainLoop::GetInstance()->media_keys_listener_manager();
    DCHECK(media_keys_listener_manager_impl);
    media_keys_listener_manager_impl->SetBrowserActiveMediaRequestId(
        request_id);

    // notify test observers we added the browser.
    if (test_observer_) {
      test_observer_->OnBrowserAdded();
    }

    return;
  }

  // At this point, we know this web contents is for a dPWA.
  // See if controls already exists for this request id.
  existing_controls = GetControlsForRequestId(request_id);

  // if the controls don't exist, we need to make an SMC and the
  // controls object.
  intptr_t window = -1;
  if (!existing_controls) {
    window = GetHWNDFromWebContents(web_contents);

    std::unique_ptr<system_media_controls::SystemMediaControls>
        system_media_controls =
            system_media_controls::SystemMediaControls::Create(
                media::AudioManager::GetGlobalAppName(), window);

    if (!system_media_controls) {
      DVLOG(1) << "WebAppSystemMediaControlsManager::OnFocusGained, "
               << "failed to create smc.";
      return;
    }

    // The global MKLM should be set as an observer for the newly created
    // SystemMediaControls object.
    system_media_controls->AddObserver(
        BrowserMainLoop::GetInstance()->media_keys_listener_manager());

    controls_map_.emplace(
        request_id,
        std::make_unique<WebAppSystemMediaControls>(
            request_id, std::move(system_media_controls),
            std::make_unique<SystemMediaControlsNotifier>(
                system_media_controls.get(), request_id),
            std::make_unique<ActiveMediaSessionController>(request_id)));

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
}

void WebAppSystemMediaControlsManager::OnRequestIdReleased(
    const base::UnguessableToken& request_id) {
  CHECK(initialized_);
  DVLOG(1) << "WebAppSystemMediaControlsManager::OnRequestIdReleased, "
              "request id = "
           << request_id;

  auto it = controls_map_.find(request_id);
  if (it == controls_map_.end()) {
    DVLOG(1) << "WebAppSystemMediaControlsManager::OnFocusLost, no match for "
                "request id = "
             << request_id;
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

void WebAppSystemMediaControlsManager::LogDataForDebugging() {
  DVLOG(1) << "WebAppSystemMediaControlsManager::LogDataForDebugging";
  int i = 0;
  for (auto& it : controls_map_) {
    DVLOG(1) << "Entry " << ++i << " "
             << "Request ID: " << it.first;

    if (it.second->GetSystemMediaControls()) {
      DVLOG(1) << "SystemMediaControls: "
               << it.second->GetSystemMediaControls();
    } else {
      DVLOG(1) << "SystemMediaControls: nullptr";
    }

    if (it.second->GetNotifier()) {
      DVLOG(1) << "Notifier: " << it.second->GetNotifier();
    } else {
      DVLOG(1) << "Notifier: nullptr";
    }

    if (it.second->GetController()) {
      DVLOG(1) << "Controller: " << it.second->GetController();
    } else {
      DVLOG(1) << "Controller: nullptr";
    }
  }
}

}  // namespace content
