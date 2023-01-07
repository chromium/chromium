// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_AUDIO_FOCUS_TEST_UTIL_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_AUDIO_FOCUS_TEST_UTIL_H_

#include <vector>

#include "base/component_export.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {
namespace test {

class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) TestAudioFocusObserver
    : public mojom::AudioFocusObserver {
 public:
  TestAudioFocusObserver();
  ~TestAudioFocusObserver() override;

  void OnFocusGained(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnFocusLost(
      media_session::mojom::AudioFocusRequestStatePtr session) override;
  void OnRequestIdReleased(const base::UnguessableToken& request_id) override {}

  void WaitForGainedEvent();
  void WaitForLostEvent();

  mojo::PendingRemote<media_session::mojom::AudioFocusObserver>
  BindNewPipeAndPassRemote();

  const media_session::mojom::AudioFocusRequestStatePtr& focus_gained_session()
      const {
    return focus_gained_session_;
  }

  const media_session::mojom::AudioFocusRequestStatePtr& focus_lost_session()
      const {
    return focus_lost_session_;
  }

  // These store the order of notifications that were received by the observer.
  enum class NotificationType {
    kFocusGained,
    kFocusLost,
  };
  const std::vector<NotificationType>& notifications() const {
    return notifications_;
  }

 private:
  mojo::Receiver<mojom::AudioFocusObserver> receiver_{this};

  // These store the values we received.
  media_session::mojom::AudioFocusRequestStatePtr focus_gained_session_;
  media_session::mojom::AudioFocusRequestStatePtr focus_lost_session_;

  // If either of these are true we will quit the run loop if we observe a gain
  // or lost event.
  bool wait_for_gained_ = false;
  bool wait_for_lost_ = false;

  std::vector<NotificationType> notifications_;

  base::RunLoop run_loop_;
};

COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP)
media_session::mojom::MediaSessionInfoPtr GetMediaSessionInfoSync(
    media_session::mojom::MediaSession*);

}  // namespace test
}  // namespace media_session

#endif  // SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_AUDIO_FOCUS_TEST_UTIL_H_
