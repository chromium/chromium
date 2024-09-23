// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_MEDIA_SESSION_H_
#define CONTENT_PUBLIC_TEST_MOCK_MEDIA_SESSION_H_

#include "base/unguessable_token.h"
#include "content/public/browser/media_session.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class MockMediaSession : public MediaSession {
 public:
  MockMediaSession();
  ~MockMediaSession() override;

  MOCK_METHOD(void,
              DidReceiveAction,
              (media_session::mojom::MediaSessionAction action),
              (override));
  MOCK_METHOD(void,
              SetDuckingVolumeMultiplier,
              (double multiplier),
              (override));
  MOCK_METHOD(void,
              SetAudioFocusGroupId,
              (const base::UnguessableToken& group_id),
              (override));
  MOCK_METHOD(void, Suspend, (SuspendType suspend_type), (override));
  MOCK_METHOD(void, Resume, (SuspendType suspend_type), (override));
  MOCK_METHOD(void, StartDucking, (), (override));
  MOCK_METHOD(void, StopDucking, (), (override));
  MOCK_METHOD(void,
              GetMediaSessionInfo,
              (GetMediaSessionInfoCallback callback),
              (override));
  MOCK_METHOD(void, GetDebugInfo, (GetDebugInfoCallback callback), (override));
  MOCK_METHOD(void,
              AddObserver,
              (mojo::PendingRemote<media_session::mojom::MediaSessionObserver>
                   observer),
              (override));
  MOCK_METHOD(void, PreviousTrack, (), (override));
  MOCK_METHOD(void, NextTrack, (), (override));
  MOCK_METHOD(void, SkipAd, (), (override));
  MOCK_METHOD(void, Seek, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, Stop, (SuspendType suspend_type), (override));
  MOCK_METHOD(void,
              GetMediaImageBitmap,
              (const media_session::MediaImage& image,
               int minimum_size_px,
               int desired_size_px,
               GetMediaImageBitmapCallback callback),
              (override));
  MOCK_METHOD(void, SeekTo, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, ScrubTo, (base::TimeDelta seek_time), (override));
  MOCK_METHOD(void, EnterPictureInPicture, (), (override));
  MOCK_METHOD(void, ExitPictureInPicture, (), (override));
  MOCK_METHOD(void,
              GetVisibility,
              (GetVisibilityCallback callback),
              (override));
  MOCK_METHOD(void,
              SetAudioSinkId,
              (const std::optional<std::string>& id),
              (override));
  MOCK_METHOD(void, ToggleMicrophone, (), (override));
  MOCK_METHOD(void, ToggleCamera, (), (override));
  MOCK_METHOD(void, HangUp, (), (override));
  MOCK_METHOD(void, Raise, (), (override));
  MOCK_METHOD(void, SetMute, (bool mute), (override));
  MOCK_METHOD(void, RequestMediaRemoting, (), (override));
  MOCK_METHOD(void, PreviousSlide, (), (override));
  MOCK_METHOD(void, NextSlide, (), (override));
  MOCK_METHOD(void, EnterAutoPictureInPicture, (), (override));
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_MEDIA_SESSION_H_
