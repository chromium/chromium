// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_AUDIO_FOCUS_MANAGER_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_AUDIO_FOCUS_MANAGER_H_

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media_session {
namespace test {

class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) MockAudioFocusManager
    : public mojom::AudioFocusManager {
 public:
  MockAudioFocusManager();
  MockAudioFocusManager(const MockAudioFocusManager&) = delete;
  MockAudioFocusManager& operator=(const MockAudioFocusManager&) = delete;
  ~MockAudioFocusManager() override;

  mojo::PendingRemote<mojom::AudioFocusManager> GetPendingRemote();

  void Flush();

  // mojom::AudioFocusManager:
  MOCK_METHOD(void,
              RequestAudioFocus,
              (mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
               mojo::PendingRemote<mojom::MediaSession> session,
               mojom::MediaSessionInfoPtr session_info,
               mojom::AudioFocusType type,
               RequestAudioFocusCallback callback));
  MOCK_METHOD(void,
              RequestGroupedAudioFocus,
              (const base::UnguessableToken& request_id,
               mojo::PendingReceiver<mojom::AudioFocusRequestClient> receiver,
               mojo::PendingRemote<mojom::MediaSession> session,
               mojom::MediaSessionInfoPtr session_info,
               mojom::AudioFocusType type,
               const base::UnguessableToken& group_id,
               RequestGroupedAudioFocusCallback callback));
  MOCK_METHOD(void, GetFocusRequests, (GetFocusRequestsCallback callback));
  MOCK_METHOD(void,
              AddObserver,
              (mojo::PendingRemote<mojom::AudioFocusObserver> observer));
  MOCK_METHOD(void,
              SetSource,
              (const base::UnguessableToken& identity,
               const std::string& name));
  MOCK_METHOD(void, SetEnforcementMode, (mojom::EnforcementMode mode));
  MOCK_METHOD(void,
              AddSourceObserver,
              (const base::UnguessableToken& source_id,
               mojo::PendingRemote<mojom::AudioFocusObserver> observer));
  MOCK_METHOD(void,
              GetSourceFocusRequests,
              (const base::UnguessableToken& source_id,
               GetFocusRequestsCallback callback));
  MOCK_METHOD(void,
              RequestIdReleased,
              (const base::UnguessableToken& request_id));

 private:
  mojo::Receiver<mojom::AudioFocusManager> receiver_{this};
};

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_MOCK_AUDIO_FOCUS_MANAGER_H_
