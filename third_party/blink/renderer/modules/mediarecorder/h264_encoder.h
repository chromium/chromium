// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_H264_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_H264_ENCODER_H_

#include "base/task/sequenced_task_runner.h"
#include "media/media_buildflags.h"

#if !BUILDFLAG(ENABLE_OPENH264)
#error ENABLE_OPENH264 should be defined.
#endif  // #if BUILDFLAG(ENABLE_OPENH264)

#include "base/time/time.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/openh264/src/codec/api/wels/codec_api.h"

namespace blink {

// Class encapsulating all openh264 interactions for H264 encoding.
class MODULES_EXPORT H264Encoder final : public VideoTrackRecorder::Encoder {
 public:
  struct ISVCEncoderDeleter {
    void operator()(ISVCEncoder* codec);
  };
  typedef std::unique_ptr<ISVCEncoder, ISVCEncoderDeleter> ScopedISVCEncoderPtr;

  H264Encoder(scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
              const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
              VideoTrackRecorder::CodecProfile codec_profile,
              uint32_t bits_per_second,
              bool is_screencast,
              const VideoTrackRecorder::OnErrorCB on_error_cb);
  ~H264Encoder() override;

  H264Encoder(const H264Encoder&) = delete;
  H264Encoder& operator=(const H264Encoder&) = delete;

 private:
  friend class H264EncoderFixture;

  // VideoTrackRecorder::Encoder implementation.
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp,
                   bool request_keyframe) override;

  [[nodiscard]] bool ConfigureEncoder(const gfx::Size& size);

  SEncParamExt GetEncoderOptionForTesting();

  // TODO(inker): Move this field into VideoTrackRecorder::Encoder.
  const VideoTrackRecorder::CodecProfile codec_profile_;
  const bool is_screencast_;

  const VideoTrackRecorder::OnErrorCB on_error_cb_;

  // |openh264_encoder_| is a special scoped pointer to guarantee proper
  // destruction, also when reconfiguring due to parameters change.
  gfx::Size configured_size_;
  ScopedISVCEncoderPtr openh264_encoder_;

  // The |VideoFrame::timestamp()| of the first received frame.
  base::TimeTicks first_frame_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_H264_ENCODER_H_
