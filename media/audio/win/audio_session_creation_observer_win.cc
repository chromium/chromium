// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/win/audio_session_creation_observer_win.h"

#include <unknwn.h>

#include <audiopolicy.h>

namespace media {

AudioSessionCreationObserverWin::AudioSessionCreationObserverWin(
    base::RepeatingClosure session_created_callback)
    : session_created_cb_(session_created_callback) {}

AudioSessionCreationObserverWin::~AudioSessionCreationObserverWin() = default;

HRESULT AudioSessionCreationObserverWin::OnSessionCreated(
    IAudioSessionControl* session) {
  session_created_cb_.Run();
  return S_OK;
}

}  // namespace media
