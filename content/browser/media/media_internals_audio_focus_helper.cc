// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_audio_focus_helper.h"

#include <list>
#include <string>

#include "base/bind.h"
#include "base/containers/adapters.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

const char kAudioFocusFunction[] = "media.onReceiveAudioFocusState";
const char kAudioFocusIdKey[] = "id";
const char kAudioFocusSessionsKey[] = "sessions";

const char kAudioFocusForceDuck[] = "ForceDuck";
const char kAudioFocusPreferStop[] = "PreferStop";

const char kAudioFocusTypeGain[] = "Gain";
const char kAudioFocusTypeGainTransient[] = "GainTransient";
const char kAudioFocusTypeGainTransientMayDuck[] = "GainTransientMayDuck";
const char kAudioFocusTypeAmbient[] = "Ambient";

const char kMediaSessionStateActive[] = "Active";
const char kMediaSessionStateDucking[] = "Ducking";
const char kMediaSessionStateSuspended[] = "Suspended";
const char kMediaSessionStateInactive[] = "Inactive";

const char kMediaSessionPlaybackStatePaused[] = "Paused";
const char kMediaSessionPlaybackStatePlaying[] = "Playing";

const char kMediaSessionIsControllable[] = "Controllable";
const char kMediaSessionIsSensitive[] = "Sensitive";

}  // namespace

MediaInternalsAudioFocusHelper::MediaInternalsAudioFocusHelper() = default;

MediaInternalsAudioFocusHelper::~MediaInternalsAudioFocusHelper() = default;

void MediaInternalsAudioFocusHelper::SendAudioFocusState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!EnsureServiceConnection())
    return;

  // Get the audio focus state from the media session service.
  audio_focus_->GetFocusRequests(base::BindOnce(
      &MediaInternalsAudioFocusHelper::DidGetAudioFocusRequestList,
      base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::OnFocusGained(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MediaInternalsAudioFocusHelper::SendAudioFocusState,
                     base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MediaInternalsAudioFocusHelper::SendAudioFocusState,
                     base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  enabled_ = enabled;

  EnsureServiceConnection();

  if (!enabled) {
    audio_focus_.reset();
    audio_focus_debug_.reset();
    receiver_.reset();
  }
}

bool MediaInternalsAudioFocusHelper::EnsureServiceConnection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!enabled_)
    return false;

  // |connector| may be nullptr in some tests.
  service_manager::Connector* connector = GetSystemConnector();
  if (!connector)
    return false;

  // Connect to the media session service.
  if (!audio_focus_.is_bound()) {
    connector->Connect(media_session::mojom::kServiceName,
                       audio_focus_.BindNewPipeAndPassReceiver());
    audio_focus_.set_disconnect_handler(base::BindRepeating(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  }

  // Connect to the media session service debug interface.
  if (!audio_focus_debug_.is_bound()) {
    connector->Connect(media_session::mojom::kServiceName,
                       audio_focus_debug_.BindNewPipeAndPassReceiver());
    audio_focus_debug_.set_disconnect_handler(
        base::BindRepeating(&MediaInternalsAudioFocusHelper::OnDebugMojoError,
                            base::Unretained(this)));
  }

  // Add the observer to receive audio focus events.
  if (!receiver_.is_bound()) {
    audio_focus_->AddObserver(receiver_.BindNewPipeAndPassRemote());

    receiver_.set_disconnect_handler(base::BindRepeating(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  }

  return true;
}

void MediaInternalsAudioFocusHelper::OnMojoError() {
  audio_focus_.reset();
  receiver_.reset();
}

void MediaInternalsAudioFocusHelper::OnDebugMojoError() {
  audio_focus_debug_.reset();
}

void MediaInternalsAudioFocusHelper::DidGetAudioFocusRequestList(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> stack) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!EnsureServiceConnection())
    return;

  audio_focus_data_.Clear();
  request_state_.clear();

  // We should go backwards through the stack so the top of the stack is
  // always shown first in the list.
  base::ListValue stack_data;
  for (const auto& session : base::Reversed(stack)) {
    if (!session->request_id.has_value())
      continue;

    std::string id_string = session->request_id.value().ToString();
    base::DictionaryValue media_session_data;
    media_session_data.SetKey(kAudioFocusIdKey, base::Value(id_string));
    stack_data.Append(std::move(media_session_data));

    request_state_.emplace(id_string, session.Clone());

    audio_focus_debug_->GetDebugInfoForRequest(
        session->request_id.value(),
        base::BindOnce(
            &MediaInternalsAudioFocusHelper::DidGetAudioFocusDebugInfo,
            base::Unretained(this), id_string));
  }

  audio_focus_data_.SetKey(kAudioFocusSessionsKey, std::move(stack_data));

  // If the stack is empty then we should send an update to the web ui to clear
  // the list.
  if (stack.empty())
    SerializeAndSendUpdate(kAudioFocusFunction, &audio_focus_data_);
}

void MediaInternalsAudioFocusHelper::DidGetAudioFocusDebugInfo(
    const std::string& id,
    media_session::mojom::MediaSessionDebugInfoPtr info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!EnsureServiceConnection())
    return;

  base::Value* sessions_list =
      audio_focus_data_.FindKey(kAudioFocusSessionsKey);
  DCHECK(sessions_list);

  bool updated = false;
  for (auto& session : sessions_list->GetList()) {
    if (session.FindKey(kAudioFocusIdKey)->GetString() != id)
      continue;

    auto state = request_state_.find(id);
    DCHECK(state != request_state_.end());

    session.SetKey("name",
                   base::Value(BuildNameString(state->second, info->name)));
    session.SetKey("owner", base::Value(info->owner));
    session.SetKey("state",
                   base::Value(BuildStateString(state->second, info->state)));
    updated = true;
  }

  if (!updated)
    return;

  SerializeAndSendUpdate(kAudioFocusFunction, &audio_focus_data_);
}

void MediaInternalsAudioFocusHelper::SerializeAndSendUpdate(
    const std::string& function,
    const base::Value* value) {
  return MediaInternals::GetInstance()->SendUpdate(
      content::WebUI::GetJavascriptCall(
          function, std::vector<const base::Value*>(1, value)));
}

std::string MediaInternalsAudioFocusHelper::BuildNameString(
    const media_session::mojom::AudioFocusRequestStatePtr& state,
    const std::string& provided_name) const {
  std::stringstream stream;

  // Add the |source_name| (optional).
  if (state->source_name.has_value()) {
    stream << state->source_name.value();
    stream << ":";
  }

  // Add the |request_id|.
  stream << state->request_id.value().ToString();

  if (!provided_name.empty())
    stream << " " << provided_name;
  return stream.str();
}

std::string MediaInternalsAudioFocusHelper::BuildStateString(
    const media_session::mojom::AudioFocusRequestStatePtr& state,
    const std::string& provided_state) const {
  std::stringstream stream;

  // Convert the AudioFocusType mojo enum to a string.
  switch (state->audio_focus_type) {
    case media_session::mojom::AudioFocusType::kGain:
      stream << " " << kAudioFocusTypeGain;
      break;
    case media_session::mojom::AudioFocusType::kGainTransient:
      stream << " " << kAudioFocusTypeGainTransient;
      break;
    case media_session::mojom::AudioFocusType::kGainTransientMayDuck:
      stream << " " << kAudioFocusTypeGainTransientMayDuck;
      break;
    case media_session::mojom::AudioFocusType::kAmbient:
      stream << " " << kAudioFocusTypeAmbient;
      break;
  }

  // Convert the MediaSessionInfo::SessionState mojo enum to a string.
  switch (state->session_info->state) {
    case media_session::mojom::MediaSessionInfo::SessionState::kActive:
      stream << " " << kMediaSessionStateActive;
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kDucking:
      stream << " " << kMediaSessionStateDucking;
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kSuspended:
      stream << " " << kMediaSessionStateSuspended;
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kInactive:
      stream << " " << kMediaSessionStateInactive;
      break;
  }

  // Convert the MediaPlaybackState mojo enum to a string.
  switch (state->session_info->playback_state) {
    case media_session::mojom::MediaPlaybackState::kPaused:
      stream << " " << kMediaSessionPlaybackStatePaused;
      break;
    case media_session::mojom::MediaPlaybackState::kPlaying:
      stream << " " << kMediaSessionPlaybackStatePlaying;
      break;
  }

  // Convert the |force_duck| boolean into a string.
  if (state->session_info->force_duck)
    stream << " " << kAudioFocusForceDuck;

  // Convert the |prefer_stop_for_gain_focus_loss| boolean into a string.
  if (state->session_info->prefer_stop_for_gain_focus_loss)
    stream << " " << kAudioFocusPreferStop;

  // Convert the |is_controllable| boolean into a string.
  if (state->session_info->is_controllable)
    stream << " " << kMediaSessionIsControllable;

  // Convert the |is_sensitive| boolean into a string.
  if (state->session_info->is_sensitive)
    stream << " " << kMediaSessionIsSensitive;

  if (!provided_state.empty())
    stream << " " << provided_state;
  return stream.str();
}

}  // namespace content
