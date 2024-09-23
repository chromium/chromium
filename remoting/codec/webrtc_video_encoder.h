// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_
#define REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/webrtc/api/video/encoded_image.h"
#include "third_party/webrtc/api/video/video_codec_type.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

// A class to perform the task of encoding a continuous stream of images. The
// interface is asynchronous to enable maximum throughput.
class WebrtcVideoEncoder {
 public:
  struct FrameParams {
    // Target bitrate in kilobits per second.
    int bitrate_kbps = -1;

    // Frame duration.
    base::TimeDelta duration;

    // If set to true then the active map passed to the encoder will only
    // contain updated_region() from the current frame. Otherwise the active map
    // is not cleared before adding updated_region(), which means it will
    // contain a union of updated_region() from all frames since this flag was
    // last set. This flag is used to top-off video quality with VP8.
    bool clear_active_map = false;

    // Indicates that the encoder should encode this frame as a key frame.
    bool key_frame = false;

    // Target FPS. A value less than 0 means unset.
    int fps = -1;

    // Quantization parameters for the encoder.
    int vpx_min_quantizer = -1;
    int vpx_max_quantizer = -1;
  };

  // Information needed for sending video statistics for each encoded frame.
  // The destructor is virtual so that implementations can derive from this
  // class to attach more data to the frame.
  struct FrameStats {
    FrameStats() = default;
    FrameStats(const FrameStats&) = default;
    FrameStats& operator=(const FrameStats&) = default;
    virtual ~FrameStats() = default;

    // TODO(crbug.com/40175068): Consolidate all the per-frame statistics
    // into a single struct in remoting/protocol.
    base::TimeTicks capture_started_time;
    base::TimeTicks capture_ended_time;
    base::TimeTicks encode_started_time;
    base::TimeTicks encode_ended_time;
    base::TimeDelta send_pending_delay{base::TimeDelta::Max()};
    base::TimeDelta rtt_estimate{base::TimeDelta::Max()};
    int bandwidth_estimate_kbps = -1;

    // The screen that this frame was captured from.
    webrtc::ScreenId screen_id = webrtc::kInvalidScreenId;
  };

  struct EncodedFrame {
    EncodedFrame();
    EncodedFrame(const EncodedFrame&) = delete;
    EncodedFrame(EncodedFrame&&);
    EncodedFrame& operator=(const EncodedFrame&) = delete;
    EncodedFrame& operator=(EncodedFrame&&);
    ~EncodedFrame();

    webrtc::DesktopSize dimensions;
    rtc::scoped_refptr<webrtc::EncodedImageBuffer> data;
    bool key_frame;
    int quantizer;
    webrtc::VideoCodecType codec;
    int32_t profile = 0;

    uint32_t rtp_timestamp;
    std::unique_ptr<FrameStats> stats;
    // This rectangle in the input frame will be encoded by the encoder.
    int32_t encoded_rect_width = 0;
    int32_t encoded_rect_height = 0;
  };

  enum class EncodeResult {
    SUCCEEDED,

    // The implementation cannot handle the frame, the size exceeds the
    // capability of the codec or the implementation.
    FRAME_SIZE_EXCEEDS_CAPABILITY,

    // Undefined or unhandled error has happened. This error type should be
    // avoided. A more exact error type is preferred.
    UNKNOWN_ERROR,
  };

  // Helper function for the VPX and AOM encoders to determine the number of
  // threads needed to efficiently encode a frame based on its width.
  static int GetEncoderThreadCount(int frame_width);

  // A derived class calls EncodeCallback to return the result of an encoding
  // request. SUCCEEDED with an empty EncodedFrame (nullptr) indicates the frame
  // should be dropped (unchanged or empty frame). Otherwise EncodeResult shows
  // the errors.
  typedef base::OnceCallback<void(EncodeResult, std::unique_ptr<EncodedFrame>)>
      EncodeCallback;

  virtual ~WebrtcVideoEncoder() {}

  // Encoder configurable settings, may be provided via SDP or OOB via a
  // proprietary message.
  virtual void SetLosslessEncode(bool want_lossless_encode) {}
  virtual void SetLosslessColor(bool want_lossless_color) {}
  virtual void SetUseActiveMap(bool use_active_map) {}
  virtual void SetEncoderSpeed(int encoder_speed) {}

  // Encode an image stored in |frame|. If frame.updated_region() is empty
  // then the encoder may return a frame (e.g. to top-off previously-encoded
  // portions of the frame to higher quality) or return nullptr to indicate that
  // there is no work to do. |frame| may be nullptr, which is equivalent to a
  // frame with an empty updated_region(). |done| callback may be called
  // synchronously. It must not be called if the encoder is destroyed while
  // the request is pending.
  virtual void Encode(std::unique_ptr<webrtc::DesktopFrame> frame,
                      const FrameParams& param,
                      EncodeCallback done) = 0;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_WEBRTC_VIDEO_ENCODER_H_
