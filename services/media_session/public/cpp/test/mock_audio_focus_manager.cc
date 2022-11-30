// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/mock_audio_focus_manager.h"

namespace media_session {
namespace test {

MockAudioFocusManager::MockAudioFocusManager() = default;

MockAudioFocusManager::~MockAudioFocusManager() = default;

mojo::PendingRemote<mojom::AudioFocusManager>
MockAudioFocusManager::GetPendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

void MockAudioFocusManager::Flush() {
  receiver_.FlushForTesting();
}

}  // namespace test
}  // namespace media_session
