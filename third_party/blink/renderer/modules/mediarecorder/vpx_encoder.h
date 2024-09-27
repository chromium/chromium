// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VPX_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VPX_ENCODER_H_

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/modules/mediarecorder/video_track_recorder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_encoder.h"

namespace blink {

// Class encapsulating all libvpx interactions for VP8/VP9 encoding.
class VpxEncoder final : public VideoTrackRecorder::Encoder {
 public:
  // Originally from remoting/codec/scoped_vpx_codec.h.
  // TODO(mcasas): Refactor into a common location.
  struct VpxCodecDeleter {
    void operator()(vpx_codec_ctx_t* codec);
  };
  typedef std::unique_ptr<vpx_codec_ctx_t, VpxCodecDeleter>
      ScopedVpxCodecCtxPtr;

  VpxEncoder(scoped_refptr<base::SequencedTaskRunner> encoding_task_runner,
             bool use_vp9,
             const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
             uint32_t bits_per_second,
             bool is_screencast,
             const VideoTrackRecorder::OnErrorCB on_error_cb);
  VpxEncoder(const VpxEncoder&) = delete;
  VpxEncoder& operator=(const VpxEncoder&) = delete;

 private:
  // VideoTrackRecorder::Encoder implementation.
  void EncodeFrame(scoped_refptr<media::VideoFrame> frame,
                   base::TimeTicks capture_timestamp,
                   bool request_keyframe) override;
  bool CanEncodeAlphaChannel() const override;

  [[nodiscard]] bool ConfigureEncoder(const gfx::Size& size,
                                      vpx_codec_enc_cfg_t* codec_config,
                                      ScopedVpxCodecCtxPtr* encoder);

  // This function creates a scoped_refptr<media::DecoderBuffer> that is passed
  // as an out parameter. Note that this will NOT be the case for when is_alpha
  // is true, as it is expected that the scoped_refptr<media::DecoderBuffer> is
  // already populated.
  void DoEncode(vpx_codec_ctx_t* const encoder,
                const gfx::Size& frame_size,
                const uint8_t* data,
                const uint8_t* y_plane,
                int y_stride,
                const uint8_t* u_plane,
                int u_stride,
                const uint8_t* v_plane,
                int v_stride,
                const base::TimeDelta& duration,
                bool force_keyframe,
                scoped_refptr<media::DecoderBuffer>* output_data,
                bool is_alpha,
                vpx_img_fmt_t img_fmt);

  // Returns true if |codec_config| has been filled in at least once.
  bool IsInitialized(const vpx_codec_enc_cfg_t& codec_config) const;

  // Estimate the frame duration from |frame| and |last_frame_timestamp_|.
  base::TimeDelta EstimateFrameDuration(const media::VideoFrame& frame);

  // Force usage of VP9 for encoding, instead of VP8 which is the default.
  const bool use_vp9_;
  const bool is_screencast_;

  const VideoTrackRecorder::OnErrorCB on_error_cb_;

  // VPx internal objects: configuration and encoder. |encoder_| is a special
  // scoped pointer to guarantee proper destruction, particularly when
  // reconfiguring due to parameters change. Only used on
  // VideoTrackRecorder::Encoder::encoding_thread_.
  vpx_codec_enc_cfg_t codec_config_;
  ScopedVpxCodecCtxPtr encoder_;

  vpx_codec_enc_cfg_t alpha_codec_config_;
  ScopedVpxCodecCtxPtr alpha_encoder_;

  Vector<uint8_t> alpha_dummy_planes_;
  size_t v_plane_offset_;
  size_t u_plane_stride_;
  size_t v_plane_stride_;
  bool last_frame_had_alpha_ = false;

  // The |media::VideoFrame::timestamp()| of the last encoded frame.  This is
  // used to predict the duration of the next frame.
  base::TimeDelta last_frame_timestamp_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_VPX_ENCODER_H_
