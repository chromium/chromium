// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_ENCODING_SIZE_ADAPTABLE_VIDEO_ENCODER_BASE_H_
#define MEDIA_CAST_ENCODING_SIZE_ADAPTABLE_VIDEO_ENCODER_BASE_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"
#include "media/cast/encoding/video_encoder.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class VideoEncoderMetricsProvider;

namespace cast {

struct SenderEncodedFrame;

// Creates and owns a VideoEncoder instance.  The owned instance is an
// implementation that does not support changing frame sizes, and so
// SizeAdaptableVideoEncoderBase acts as a proxy to automatically detect when
// the owned instance should be replaced with one that can handle the new frame
// size.
class SizeAdaptableVideoEncoderBase : public VideoEncoder {
 public:
  SizeAdaptableVideoEncoderBase(
      const scoped_refptr<CastEnvironment>& cast_environment,
      const FrameSenderConfig& video_config,
      std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider,
      StatusChangeCallback status_change_cb);

  SizeAdaptableVideoEncoderBase(const SizeAdaptableVideoEncoderBase&) = delete;
  SizeAdaptableVideoEncoderBase& operator=(
      const SizeAdaptableVideoEncoderBase&) = delete;

  ~SizeAdaptableVideoEncoderBase() override;

  // VideoEncoder implementation.
  bool EncodeVideoFrame(scoped_refptr<media::VideoFrame> video_frame,
                        base::TimeTicks reference_time,
                        FrameEncodedCallback frame_encoded_callback) final;
  void SetBitRate(int new_bit_rate) final;
  void GenerateKeyFrame() final;
  std::unique_ptr<VideoFrameFactory> CreateVideoFrameFactory() final;
  void EmitFrames() final;

 protected:
  // Accessors for subclasses.
  CastEnvironment* cast_environment() const { return cast_environment_.get(); }
  const FrameSenderConfig& video_config() const { return video_config_; }
  const gfx::Size& frame_size() const { return frame_size_; }
  FrameId next_frame_id() const { return next_frame_id_; }
  VideoEncoderMetricsProvider& metrics_provider() const {
    return *metrics_provider_.get();
  }

  // Returns a callback that calls OnEncoderStatusChange().  The callback is
  // canceled by invalidating its bound weak pointer just before a replacement
  // encoder is instantiated.  In this scheme, OnEncoderStatusChange() can only
  // be called by the most-recent encoder.
  StatusChangeCallback CreateEncoderStatusChangeCallback();

  // Overridden by subclasses to create a new encoder instance that handles
  // frames of the size specified by |frame_size()|.
  virtual std::unique_ptr<VideoEncoder> CreateEncoder() = 0;

  // Overridden by subclasses to perform additional steps when
  // |replacement_encoder| becomes the active encoder.
  virtual void OnEncoderReplaced(VideoEncoder* replacement_encoder);

  // Overridden by subclasses to perform additional steps before/after the
  // current encoder is destroyed.
  virtual void DestroyEncoder();

 private:
  // Create and initialize a replacement video encoder, if this not already
  // in-progress.  The replacement will call back to OnEncoderStatusChange()
  // with success/fail status.
  void TrySpawningReplacementEncoder(const gfx::Size& size_needed);

  // Called when a status change is received from an encoder.
  void OnEncoderStatusChange(OperationalStatus status);

  // Called by the |encoder_| with the next EncodedFrame.
  void OnEncodedVideoFrame(FrameEncodedCallback frame_encoded_callback,
                           std::unique_ptr<SenderEncodedFrame> encoded_frame);

  const scoped_refptr<CastEnvironment> cast_environment_;

  // This is not const since |video_config_.starting_bitrate| is modified by
  // SetBitRate(), for when a replacement encoder is spawned.
  FrameSenderConfig video_config_;

  const std::unique_ptr<VideoEncoderMetricsProvider> metrics_provider_;

  // Run whenever the underlying encoder reports a status change.
  const StatusChangeCallback status_change_cb_;

  // The underlying platform video encoder and the frame size it expects.
  std::unique_ptr<VideoEncoder> encoder_;
  gfx::Size frame_size_;

  // The number of frames in |encoder_|'s pipeline.  If this is set to
  // kEncoderIsInitializing, |encoder_| is not yet ready to accept frames.
  enum { kEncoderIsInitializing = -1 };
  int frames_in_encoder_;

  // The ID for the next frame to be emitted.
  FrameId next_frame_id_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<SizeAdaptableVideoEncoderBase> weak_factory_{this};
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_ENCODING_SIZE_ADAPTABLE_VIDEO_ENCODER_BASE_H_
