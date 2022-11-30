// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/test/mock_media_controller_manager.h"

namespace media_session {
namespace test {

MockMediaControllerManager::MockMediaControllerManager() = default;

MockMediaControllerManager::~MockMediaControllerManager() = default;

mojo::PendingRemote<mojom::MediaControllerManager>
MockMediaControllerManager::GetPendingRemote() {
  return receiver_.BindNewPipeAndPassRemote();
}

}  // namespace test
}  // namespace media_session
