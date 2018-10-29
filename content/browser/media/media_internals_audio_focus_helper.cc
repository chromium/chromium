// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_internals_audio_focus_helper.h"

#include <list>
#include <string>

#include "base/containers/adapters.h"
#include "base/task/post_task.h"
#include "base/values.h"
#include "content/browser/media/media_internals.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

namespace {

const char kAudioFocusFunction[] = "media.onReceiveAudioFocusState";
const char kAudioFocusIdKey[] = "id";
const char kAudioFocusSessionsKey[] = "sessions";

}  // namespace

MediaInternalsAudioFocusHelper::MediaInternalsAudioFocusHelper() = default;

MediaInternalsAudioFocusHelper::~MediaInternalsAudioFocusHelper() = default;

void MediaInternalsAudioFocusHelper::SendAudioFocusState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!CanUpdate())
    return;

  // Get the audio focus state from the media session service.
  audio_focus_ptr_->GetFocusRequests(base::BindOnce(
      &MediaInternalsAudioFocusHelper::DidGetAudioFocusRequestList,
      base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::OnFocusGained(
    media_session::mojom::MediaSessionInfoPtr media_session,
    media_session::mojom::AudioFocusType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MediaInternalsAudioFocusHelper::SendAudioFocusState,
                     base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::OnFocusLost(
    media_session::mojom::MediaSessionInfoPtr media_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&MediaInternalsAudioFocusHelper::SendAudioFocusState,
                     base::Unretained(this)));
}

void MediaInternalsAudioFocusHelper::SetEnabled(bool enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (enabled_ == enabled)
    return;

  enabled_ = enabled;

  EnsureServiceConnection();

  if (enabled) {
    // Add the observer to receive audio focus events.
    media_session::mojom::AudioFocusObserverPtr observer;
    binding_.Bind(mojo::MakeRequest(&observer));
    audio_focus_ptr_->AddObserver(std::move(observer));

    binding_.set_connection_error_handler(base::BindRepeating(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  } else {
    audio_focus_ptr_.reset();
    audio_focus_debug_ptr_.reset();
    binding_.Close();
  }
}

void MediaInternalsAudioFocusHelper::EnsureServiceConnection() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |connection| and |connector| may be nullptr in some tests.
  ServiceManagerConnection* connection =
      ServiceManagerConnection::GetForProcess();
  if (!connection)
    return;

  service_manager::Connector* connector = connection->GetConnector();
  if (!connector)
    return;

  // Connect to the media session service.
  if (!audio_focus_ptr_.is_bound()) {
    connector->BindInterface(media_session::mojom::kServiceName,
                             mojo::MakeRequest(&audio_focus_ptr_));
    audio_focus_ptr_.set_connection_error_handler(base::BindRepeating(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  }

  // Connect to the media session service debug interface.
  if (!audio_focus_debug_ptr_.is_bound()) {
    connector->BindInterface(media_session::mojom::kServiceName,
                             mojo::MakeRequest(&audio_focus_debug_ptr_));
    audio_focus_debug_ptr_.set_connection_error_handler(base::BindRepeating(
        &MediaInternalsAudioFocusHelper::OnMojoError, base::Unretained(this)));
  }
}

void MediaInternalsAudioFocusHelper::OnMojoError() {
  audio_focus_ptr_.reset();
  audio_focus_debug_ptr_.reset();
  binding_.Close();
}

void MediaInternalsAudioFocusHelper::DidGetAudioFocusRequestList(
    std::vector<media_session::mojom::AudioFocusRequestStatePtr> stack) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!CanUpdate())
    return;

  audio_focus_data_.Clear();

  // We should go backwards through the stack so the top of the stack is
  // always shown first in the list.
  base::ListValue stack_data;
  for (const auto& session : base::Reversed(stack)) {
    if (!session->request_id.has_value())
      continue;

    std::string id_string = session->request_id.value().ToString();
    base::DictionaryValue media_session_data;
    media_session_data.SetKey(kAudioFocusIdKey, base::Value(id_string));
    stack_data.GetList().push_back(std::move(media_session_data));

    audio_focus_debug_ptr_->GetDebugInfoForRequest(
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

  if (!CanUpdate())
    return;

  base::Value* sessions_list =
      audio_focus_data_.FindKey(kAudioFocusSessionsKey);
  DCHECK(sessions_list);

  bool updated = false;
  for (auto& session : sessions_list->GetList()) {
    if (session.FindKey(kAudioFocusIdKey)->GetString() != id)
      continue;

    session.SetKey("name", base::Value(info->name));
    session.SetKey("owner", base::Value(info->owner));
    session.SetKey("state", base::Value(info->state));
    updated = true;
  }

  if (!updated)
    return;

  SerializeAndSendUpdate(kAudioFocusFunction, &audio_focus_data_);
}

bool MediaInternalsAudioFocusHelper::CanUpdate() const {
  return enabled_ && audio_focus_ptr_.is_bound() &&
         audio_focus_debug_ptr_.is_bound();
}

void MediaInternalsAudioFocusHelper::SerializeAndSendUpdate(
    const std::string& function,
    const base::Value* value) {
  return MediaInternals::GetInstance()->SendUpdate(
      content::WebUI::GetJavascriptCall(
          function, std::vector<const base::Value*>(1, value)));
}

}  // namespace content
