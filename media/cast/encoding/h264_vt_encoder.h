// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_H264_VT_ENCODER_H_
#define MEDIA_CAST_ENCODING_H264_VT_ENCODER_H_

#include <stdint.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread_checker.h"
#include "media/base/mac/videotoolbox_helpers.h"
#include "media/cast/encoding/size_adaptable_video_encoder_base.h"
#include "media/cast/encoding/video_encoder.h"

namespace media {
namespace cast {

// VideoToolbox implementation of the media::cast::VideoEncoder interface.
// VideoToolbox makes no guarantees that it is thread safe, so this object is
// pinned to the thread on which it is constructed. Supports changing frame
// sizes directly. Implements the base::PowerObserver interface to reset the
// compression session when the host process is suspended.
class H264VideoToolboxEncoder final : public VideoEncoder,
                                      public base::PowerSuspendObserver {
 public:
  // Returns true if the current platform and system configuration supports
  // using H264VideoToolboxEncoder with the given |video_config|.
  static bool IsSupported(const FrameSenderConfig& video_config);

  H264VideoToolboxEncoder(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
      StatusChangeCallback status_change_cb);

  H264VideoToolboxEncoder(const H264VideoToolboxEncoder&) = delete;
  H264VideoToolboxEncoder& operator=(const H264VideoToolboxEncoder&) = delete;

  ~H264VideoToolboxEncoder() override;

  // media::cast::VideoEncoder implementation
  bool EncodeVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                        base::TimeTicks reference_time,
                        FrameEncodedCallback frame_encoded_callback) override;
  void SetBitRate(int new_bit_rate) override;
  void GenerateKeyFrame() override;
  std::unique_ptr<VideoFrameFactory> CreateVideoFrameFactory() override;
  void EmitFrames() override;

  // base::PowerSuspendObserver
  void OnSuspend() override;
  void OnResume() override;

 private:
  // Container for the associated data of a video frame being processed.
  struct InProgressH264VTFrameEncode;
  // VideoFrameFactory tied to the VideoToolbox encoder.
  class VideoFrameFactoryImpl;

  // Reset the encoder's compression session by destroying the existing one
  // using DestroyCompressionSession() and creating a new one. The new session
  // is configured using ConfigureCompressionSession().
  void ResetCompressionSession();

  // Configure the current compression session using current encoder settings.
  void ConfigureCompressionSession();

  // Destroy the current compression session if any. Blocks until all pending
  // frames have been flushed out (similar to EmitFrames without doing any
  // encoding work).
  void DestroyCompressionSession();

  // Update the encoder's target frame size by resetting the compression
  // session. This will also update the video frame factory.
  void UpdateFrameSize(const gfx::Size& size_needed);

  // Compression session callback function to handle compressed frames. This can
  // be called by any thread.
  static void CompressionCallback(void* encoder_opaque,
                                  void* request_opaque,
                                  OSStatus status,
                                  VTEncodeInfoFlags info,
                                  CMSampleBufferRef sbuf);
  // Invoked from CompressionCallback() to handle an encoded frame on the Cast
  // main thread.
  void CompressionCallbackTask(
      std::unique_ptr<InProgressH264VTFrameEncode> request,
      OSStatus status,
      bool is_keyframe,
      std::string data);

  // The cast environment (contains worker threads & more).
  const scoped_refptr<CastEnvironment> cast_environment_;

  // VideoSenderConfig copy so we can create compression sessions on demand.
  // This is needed to recover from backgrounding and other events that can
  // invalidate compression sessions.
  const FrameSenderConfig video_config_;

  // The VideoToolbox does not support bitrate configuration, so we use a
  // constant bitrate determined on construction.
  const int average_bitrate_;

  // Frame size of the current compression session. Can be changed by submitting
  // a frame of a different size, which will cause a compression session reset.
  gfx::Size frame_size_;

  const std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;

  // Callback used to report initialization status and runtime errors.
  const StatusChangeCallback status_change_cb_;

  // Thread checker to enforce that this object is used on cast main thread.
  THREAD_CHECKER(thread_checker_);

  // The compression session.
  base::apple::ScopedCFTypeRef<VTCompressionSessionRef> compression_session_;

  // Video frame factory tied to the encoder.
  scoped_refptr<VideoFrameFactoryImpl> video_frame_factory_;

  // The ID for the next frame to be emitted.
  FrameId next_frame_id_;

  // Force next frame to be a keyframe.
  bool encode_next_frame_as_keyframe_;

  // Power suspension state.
  bool power_suspended_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<H264VideoToolboxEncoder> weak_factory_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_H264_VT_ENCODER_H_
