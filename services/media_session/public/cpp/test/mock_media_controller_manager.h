// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_CONTROLLER_MANAGER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_CONTROLLER_MANAGER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_session {
namespace test {

class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
    MockMediaControllerManager : public mojom::MediaControllerManager {
 public:
  MockMediaControllerManager();
  MockMediaControllerManager(const MockMediaControllerManager&) = delete;
  MockMediaControllerManager& operator=(const MockMediaControllerManager&) =
      delete;
  ~MockMediaControllerManager() override;

  mojo::PendingRemote<mojom::MediaControllerManager> GetPendingRemote();

  // mojom::MediaControllerManager:
  MOCK_METHOD(void,
              CreateActiveMediaController,
              (mojo::PendingReceiver<mojom::MediaController> receiver));
  MOCK_METHOD(void,
              CreateMediaControllerForSession,
              (mojo::PendingReceiver<mojom::MediaController> receiver,
               const base::UnguessableToken& receiver_id));
  MOCK_METHOD(void, SuspendAllSessions, ());

 private:
  mojo::Receiver<mojom::MediaControllerManager> receiver_{this};
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_MEDIA_CONTROLLER_MANAGER_H_
