// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_AUDIO_FOCUS_TEST_UTIL_H_
#define SERVICES_MEDIA_SESSION_PUBLIC_CPP_TEST_AUDIO_FOCUS_TEST_UTIL_H_

#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"

namespace media_session {
namespace test {

class COMPONENT_EXPORT(MEDIA_SESSION_TEST_SUPPORT_CPP) TestAudioFocusObserver
    : public mojom::AudioFocusObserver {
 public:
  TestAudioFocusObserver();
  ~TestAudioFocusObserver() override;

  void OnFocusGained(media_session::mojom::MediaSessionInfoPtr,
                     media_session::mojom::AudioFocusType) override;

  void OnFocusLost(media_session::mojom::MediaSessionInfoPtr) override;

  void WaitForGainedEvent();
  void WaitForLostEvent();

  media_session::mojom::AudioFocusType focus_gained_type() const {
    DCHECK(!focus_gained_session_.is_null());
    return focus_gained_type_;
  }

  void BindToMojoRequest(media_session::mojom::AudioFocusObserverRequest);

  // These store the values we received.
  media_session::mojom::MediaSessionInfoPtr focus_gained_session_;
  media_session::mojom::MediaSessionInfoPtr focus_lost_session_;

  // These store the order of notifications that were received by the observer.
  enum class NotificationType {
    kFocusGained,
    kFocusLost,
  };
  const std::vector<NotificationType>& notifications() const {
    return notifications_;
  }

 private:
  mojo::Binding<mojom::AudioFocusObserver> binding_;
  media_session::mojom::AudioFocusType focus_gained_type_;

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
