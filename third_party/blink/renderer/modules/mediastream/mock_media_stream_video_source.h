// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_

#include "third_party/blink/public/web/modules/mediastream/media_stream_video_source.h"

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace blink {

class MockMediaStreamVideoSource : public blink::MediaStreamVideoSource {
 public:
  MockMediaStreamVideoSource();
  explicit MockMediaStreamVideoSource(bool respond_to_request_refresh_frame);
  MockMediaStreamVideoSource(const media::VideoCaptureFormat& format,
                             bool respond_to_request_refresh_frame);
  ~MockMediaStreamVideoSource() override;

  MOCK_METHOD1(DoSetMutedState, void(bool muted_state));
  MOCK_METHOD0(OnEncodedSinkEnabled, void());
  MOCK_METHOD0(OnEncodedSinkDisabled, void());
  MOCK_METHOD0(OnRequestRefreshFrame, void());
  MOCK_METHOD1(OnCapturingLinkSecured, void(bool));
  MOCK_METHOD1(SetCanDiscardAlpha, void(bool can_discard_alpha));
  MOCK_CONST_METHOD0(SupportsEncodedOutput, bool());
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason));
  MOCK_CONST_METHOD1(OnFrameFeedback, void(const media::VideoCaptureFeedback&));

  // Simulate that the underlying source start successfully.
  void StartMockedSource();

  // Simulate that the underlying source fail to start.
  void FailToStartMockedSource();

  // Returns true if StartSource  has been called and StartMockedSource
  // or FailToStartMockedSource has not been called.
  bool SourceHasAttemptedToStart() { return attempted_to_start_; }

  // Delivers |frame| to all registered tracks on the IO thread. Its up to the
  // caller to make sure MockMediaStreamVideoSource is not destroyed before the
  // frame has been delivered.
  void DeliverVideoFrame(scoped_refptr<media::VideoFrame> frame);

  // Delivers |frame| to all registered encoded sinks on the IO thread. Its up
  // to the caller to make sure MockMediaStreamVideoSource is not destroyed
  // before the frame has been delivered.
  void DeliverEncodedVideoFrame(scoped_refptr<EncodedVideoFrame> frame);

  const media::VideoCaptureFormat& start_format() const { return format_; }
  int max_requested_height() const { return max_requested_height_; }
  int max_requested_width() const { return max_requested_width_; }
  double max_requested_frame_rate() const { return max_requested_frame_rate_; }

  void SetMutedState(bool muted_state) override {
    blink::MediaStreamVideoSource::SetMutedState(muted_state);
    DoSetMutedState(muted_state);
  }

  void EnableStopForRestart() { can_stop_for_restart_ = true; }
  void DisableStopForRestart() { can_stop_for_restart_ = false; }

  void EnableRestart() { can_restart_ = true; }
  void DisableRestart() { can_restart_ = false; }

  bool is_suspended() { return is_suspended_; }

  // Implements blink::MediaStreamVideoSource.
  void RequestRefreshFrame() override;
  absl::optional<media::VideoCaptureParams> GetCurrentCaptureParams()
      const override;
  void OnHasConsumers(bool has_consumers) override;
  VideoCaptureFeedbackCB GetFeedbackCallback() const override;
  base::WeakPtr<MediaStreamVideoSource> GetWeakPtr() const override;

 protected:
  // Implements MediaStreamSource.
  void DoChangeSource(const blink::MediaStreamDevice& new_device) override;

  // Implements blink::MediaStreamVideoSource.
  void StartSourceImpl(VideoCaptureDeliverFrameCB frame_callback,
                       EncodedVideoFrameCB encoded_frame_callback) override;
  void StopSourceImpl() override;
  absl::optional<media::VideoCaptureFormat> GetCurrentFormat() const override;
  void StopSourceForRestartImpl() override;
  void RestartSourceImpl(const media::VideoCaptureFormat& new_format) override;

 private:
  media::VideoCaptureFormat format_;
  bool respond_to_request_refresh_frame_;
  int max_requested_height_;
  int max_requested_width_;
  double max_requested_frame_rate_;
  bool attempted_to_start_;
  bool is_stopped_for_restart_ = false;
  bool can_stop_for_restart_ = true;
  bool can_restart_ = true;
  bool is_suspended_ = false;
  blink::VideoCaptureDeliverFrameCB frame_callback_;
  EncodedVideoFrameCB encoded_frame_callback_;

  base::WeakPtrFactory<MediaStreamVideoSource> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamVideoSource);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MOCK_MEDIA_STREAM_VIDEO_SOURCE_H_
