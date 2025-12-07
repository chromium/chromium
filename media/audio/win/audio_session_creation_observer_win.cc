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

ULONG AudioSessionCreationObserverWin::AddRef() {
  return 1;
}

ULONG AudioSessionCreationObserverWin::Release() {
  return 1;
}

HRESULT AudioSessionCreationObserverWin::QueryInterface(REFIID iid,
                                                        void** object) {
  if (iid == IID_IUnknown || iid == __uuidof(IAudioSessionNotification)) {
    *object = static_cast<IAudioSessionNotification*>(this);
    return S_OK;
  }

  *object = nullptr;
  return E_NOINTERFACE;
}

HRESULT AudioSessionCreationObserverWin::OnSessionCreated(
    IAudioSessionControl* session) {
  session_created_cb_.Run();
  return S_OK;
}

}  // namespace media
