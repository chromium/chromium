// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_audio_focus_helper.h"

#include <string>
#include <string_view>

#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/web_ui.h"

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

const char kMediaSessionHasAudio[] = "HasAudio";
const char kMediaSessionHasVideo[] = "HasVideo";
const char kMediaSessionHasAudioVideo[] = "HasAudioVideo";

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

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaInternalsAudioFocusHelper::SendAudioFocusState,
                     base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::OnFocusLost(
    media_session::mojom::AudioFocusRequestStatePtr session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
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

  // Connect to the media session service.
  if (!audio_focus_.is_bound()) {
    GetMediaSessionService().BindAudioFocusManager(
        audio_focus_.BindNewPipeAndPassReceiver());
    audio_focus_.set_disconnect_handler(base::BindOnce(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  }

  // Connect to the media session service debug interface.
  if (!audio_focus_debug_.is_bound()) {
    GetMediaSessionService().BindAudioFocusManagerDebug(
        audio_focus_debug_.BindNewPipeAndPassReceiver());
    audio_focus_debug_.set_disconnect_handler(
        base::BindOnce(&MediaInternalsAudioFocusHelper::OnDebugMojoError,
                       base::Unretained(this)));
  }

  // Add the observer to receive audio focus events.
  if (!receiver_.is_bound()) {
    audio_focus_->AddObserver(receiver_.BindNewPipeAndPassRemote());

    receiver_.set_disconnect_handler(base::BindOnce(
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

  audio_focus_data_.clear();
  request_state_.clear();

  // We should go backwards through the stack so the top of the stack is
  // always shown first in the list.
  base::Value::List stack_data;
  for (const auto& session : base::Reversed(stack)) {
    if (!session->request_id.has_value())
      continue;

    std::string id_string = session->request_id.value().ToString();
    base::Value::Dict media_session_data;
    media_session_data.Set(kAudioFocusIdKey, id_string);
    stack_data.Append(std::move(media_session_data));

    request_state_.emplace(id_string, session.Clone());

    audio_focus_debug_->GetDebugInfoForRequest(
        session->request_id.value(),
        base::BindOnce(
            &MediaInternalsAudioFocusHelper::DidGetAudioFocusDebugInfo,
            base::Unretained(this), id_string));
  }

  audio_focus_data_.Set(kAudioFocusSessionsKey, std::move(stack_data));

  // If the stack is empty then we should send an update to the web ui to clear
  // the list.
  if (stack.empty())
    SerializeAndSendUpdate(kAudioFocusFunction, audio_focus_data_);
}

void MediaInternalsAudioFocusHelper::DidGetAudioFocusDebugInfo(
    const std::string& id,
    media_session::mojom::MediaSessionDebugInfoPtr info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!EnsureServiceConnection())
    return;

  base::Value::List* sessions_list =
      audio_focus_data_.FindList(kAudioFocusSessionsKey);
  DCHECK(sessions_list);

  bool updated = false;
  for (auto& value : *sessions_list) {
    base::Value::Dict& session = value.GetDict();
    if (session.Find(kAudioFocusIdKey)->GetString() != id)
      continue;

    auto state = request_state_.find(id);
    CHECK(state != request_state_.end(), base::NotFatalUntil::M130);

    session.Set("name", BuildNameString(state->second, info->name));
    session.Set("owner", info->owner);
    session.Set("state", BuildStateString(state->second, info->state));
    updated = true;
  }

  if (!updated)
    return;

  SerializeAndSendUpdate(kAudioFocusFunction, audio_focus_data_);
}

void MediaInternalsAudioFocusHelper::SerializeAndSendUpdate(
    std::string_view function,
    const base::Value::Dict& value) {
  base::ValueView args[] = {value};
  return MediaInternals::GetInstance()->SendUpdate(
      content::WebUI::GetJavascriptCall(function, args));
}

std::string MediaInternalsAudioFocusHelper::BuildNameString(
    const media_session::mojom::AudioFocusRequestStatePtr& state,
    const std::string& provided_name) const {
  std::string result;

  // Add the |source_name| (optional).
  if (state->source_name.has_value())
    base::StrAppend(&result, {state->source_name.value(), ":"});

  // Add the |request_id|.
  result.append(state->request_id.value().ToString());

  if (!provided_name.empty())
    base::StrAppend(&result, {" ", provided_name});

  return result;
}

std::string MediaInternalsAudioFocusHelper::BuildStateString(
    const media_session::mojom::AudioFocusRequestStatePtr& state,
    const std::string& provided_state) const {
  std::string result(" ");

  // Convert the AudioFocusType mojo enum to a string.
  switch (state->audio_focus_type) {
    case media_session::mojom::AudioFocusType::kGain:
      result.append(kAudioFocusTypeGain);
      break;
    case media_session::mojom::AudioFocusType::kGainTransient:
      result.append(kAudioFocusTypeGainTransient);
      break;
    case media_session::mojom::AudioFocusType::kGainTransientMayDuck:
      result.append(kAudioFocusTypeGainTransientMayDuck);
      break;
    case media_session::mojom::AudioFocusType::kAmbient:
      result.append(kAudioFocusTypeAmbient);
      break;
  }

  // Convert the MediaSessionInfo::SessionState mojo enum to a string.
  result.append(" ");
  switch (state->session_info->state) {
    case media_session::mojom::MediaSessionInfo::SessionState::kActive:
      result.append(kMediaSessionStateActive);
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kDucking:
      result.append(kMediaSessionStateDucking);
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kSuspended:
      result.append(kMediaSessionStateSuspended);
      break;
    case media_session::mojom::MediaSessionInfo::SessionState::kInactive:
      result.append(kMediaSessionStateInactive);
      break;
  }

  // Convert the MediaPlaybackState mojo enum to a string.
  result.append(" ");
  switch (state->session_info->playback_state) {
    case media_session::mojom::MediaPlaybackState::kPaused:
      result.append(kMediaSessionPlaybackStatePaused);
      break;
    case media_session::mojom::MediaPlaybackState::kPlaying:
      result.append(kMediaSessionPlaybackStatePlaying);
      break;
  }

  // Convert the audio_video_states to a string.
  if (state->session_info->audio_video_states) {
    result.append(" {");
    base::ranges::for_each(
        *state->session_info->audio_video_states, [&result](const auto& state) {
          result.append(" ");
          switch (state) {
            case media_session::mojom::MediaAudioVideoState::kAudioOnly:
              result.append(kMediaSessionHasAudio);
              break;
            case media_session::mojom::MediaAudioVideoState::kVideoOnly:
              result.append(kMediaSessionHasVideo);
              break;
            case media_session::mojom::MediaAudioVideoState::kAudioVideo:
              result.append(kMediaSessionHasAudioVideo);
              break;
            case media_session::mojom::MediaAudioVideoState::kDeprecatedUnknown:
              NOTREACHED_IN_MIGRATION();
              break;
          }
        });
    result.append(" }");
  }

  // Convert the |force_duck| boolean into a string.
  if (state->session_info->force_duck)
    base::StrAppend(&result, {" ", kAudioFocusForceDuck});

  // Convert the |prefer_stop_for_gain_focus_loss| boolean into a string.
  if (state->session_info->prefer_stop_for_gain_focus_loss)
    base::StrAppend(&result, {" ", kAudioFocusPreferStop});

  // Convert the |is_controllable| boolean into a string.
  if (state->session_info->is_controllable)
    base::StrAppend(&result, {" ", kMediaSessionIsControllable});

  // Convert the |is_sensitive| boolean into a string.
  if (state->session_info->is_sensitive)
    base::StrAppend(&result, {" ", kMediaSessionIsSensitive});

  if (!provided_state.empty())
    base::StrAppend(&result, {" ", provided_state});

  return result;
}

}  // namespace content
