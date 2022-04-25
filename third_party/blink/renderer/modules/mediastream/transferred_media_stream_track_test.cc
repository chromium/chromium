// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/transferred_media_stream_track.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_capabilities.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_constraints.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_track_settings.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_track.h"

namespace blink {

class TransferredMediaStreamTrackTest : public testing::Test {
 public:
  void TearDown() override {}
};

TEST_F(TransferredMediaStreamTrackTest, InitialProperties) {
  TransferredMediaStreamTrack transferred_track(TransferredValues{
      .kind = "video",
      .id = "",
      .label = "dummy",
      .enabled = true,
      .muted = false,
      .content_hint = WebMediaStreamTrack::ContentHintType::kNone,
      .ready_state = MediaStreamSource::kReadyStateLive});

  EXPECT_EQ(transferred_track.kind(), "video");
  EXPECT_EQ(transferred_track.id(), "");
  EXPECT_EQ(transferred_track.label(), "dummy");
  EXPECT_EQ(transferred_track.enabled(), true);
  EXPECT_EQ(transferred_track.muted(), false);
  EXPECT_EQ(transferred_track.ContentHint(), "");
  EXPECT_EQ(transferred_track.readyState(), "live");
  EXPECT_EQ(transferred_track.GetReadyState(),
            MediaStreamSource::kReadyStateLive);
  EXPECT_EQ(transferred_track.Ended(), false);
  EXPECT_EQ(transferred_track.serializable_session_id(), absl::nullopt);
}

TEST_F(TransferredMediaStreamTrackTest, PropertiesInheritFromImplementation) {
  const String kKind = "audio";
  const String kId = "id";
  const String kLabel = "label";
  const bool kEnabled = false;
  const bool kMuted = true;
  const String kContentHint = "motion";
  const String kReadyState = "ended";
  const MediaStreamSource::ReadyState kReadyStateEnum =
      MediaStreamSource::kReadyStateEnded;
  const bool kEnded = true;
  const absl::optional<base::UnguessableToken> kSerializableSessionId =
      base::UnguessableToken::Create();

  Persistent<MediaTrackCapabilities> capabilities =
      MediaTrackCapabilities::Create();
  Persistent<MediaTrackConstraints> constraints =
      MediaTrackConstraints::Create();
  Persistent<MediaTrackSettings> settings = MediaTrackSettings::Create();
  Persistent<CaptureHandle> capture_handle = CaptureHandle::Create();

  MockMediaStreamTrack mock_impl;
  mock_impl.SetKind(kKind);
  mock_impl.SetId(kId);
  mock_impl.SetLabel(kLabel);
  mock_impl.setEnabled(kEnabled);
  mock_impl.SetMuted(kMuted);
  mock_impl.SetContentHint(kContentHint);
  mock_impl.SetReadyState(kReadyState);
  mock_impl.SetCapabilities(capabilities);
  mock_impl.SetConstraints(constraints);
  mock_impl.SetSettings(settings);
  mock_impl.SetCaptureHandle(capture_handle);
  mock_impl.SetReadyState(kReadyStateEnum);
  mock_impl.SetComponent(nullptr);
  mock_impl.SetEnded(kEnded);
  mock_impl.SetSerializableSessionId(kSerializableSessionId);

  TransferredMediaStreamTrack transferred_track(TransferredValues{
      .kind = "video",
      .id = "",
      .label = "dummy",
      .enabled = true,
      .muted = false,
      .content_hint = WebMediaStreamTrack::ContentHintType::kNone,
      .ready_state = MediaStreamSource::kReadyStateLive});
  transferred_track.setImplementation(&mock_impl);

  EXPECT_EQ(transferred_track.kind(), kKind);
  EXPECT_EQ(transferred_track.id(), kId);
  EXPECT_EQ(transferred_track.label(), kLabel);
  EXPECT_EQ(transferred_track.enabled(), kEnabled);
  EXPECT_EQ(transferred_track.muted(), kMuted);
  EXPECT_EQ(transferred_track.ContentHint(), kContentHint);
  EXPECT_EQ(transferred_track.readyState(), kReadyState);
  EXPECT_EQ(transferred_track.GetReadyState(), kReadyStateEnum);
  EXPECT_EQ(transferred_track.Ended(), kEnded);
  EXPECT_EQ(transferred_track.serializable_session_id(),
            kSerializableSessionId);
}

}  // namespace blink
