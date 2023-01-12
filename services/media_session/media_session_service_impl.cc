// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/media_session_service_impl.h"

#include "base/functional/bind.h"
#include "services/media_session/audio_focus_manager.h"

namespace media_session {

MediaSessionServiceImpl::MediaSessionServiceImpl()
    : audio_focus_manager_(std::make_unique<AudioFocusManager>()) {}

MediaSessionServiceImpl::~MediaSessionServiceImpl() = default;

void MediaSessionServiceImpl::BindAudioFocusManager(
    mojo::PendingReceiver<mojom::AudioFocusManager> receiver) {
  audio_focus_manager_->BindToInterface(std::move(receiver));
}

void MediaSessionServiceImpl::BindAudioFocusManagerDebug(
    mojo::PendingReceiver<mojom::AudioFocusManagerDebug> receiver) {
  audio_focus_manager_->BindToDebugInterface(std::move(receiver));
}

void MediaSessionServiceImpl::BindMediaControllerManager(
    mojo::PendingReceiver<mojom::MediaControllerManager> receiver) {
  audio_focus_manager_->BindToControllerManagerInterface(std::move(receiver));
}

}  // namespace media_session
