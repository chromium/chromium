// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <va/va.h>

#include <optional>
#include <vector>

#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/video/video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "ui/gfx/geometry/size.h"

namespace media {
struct BitstreamBufferMetadata;
class CodecPicture;
class ScopedVABuffer;
class VideoFrame;
class VaapiWrapper;

// An VaapiVideoEncoderDelegate  performs high-level, platform-independent
// encoding process tasks, such as managing codec state, reference frames, etc.,
// but may require support from an external accelerator (typically a hardware
// accelerator) to offload some stages of the actual encoding process, using
// the parameters that the Delegate prepares beforehand.
//
// For each frame to be encoded, clients provide an EncodeJob object to be set
// up by a Delegate subclass with job parameters, and execute the job
// afterwards. Any resources required for the job are also provided by the
// clients, and associated with the EncodeJob object.
class VaapiVideoEncoderDelegate {
 public:
  struct Config {
    // Maximum number of reference frames.
    // For H.264 encoding, the value represents the maximum number of reference
    // frames for both the reference picture list 0 (bottom 16 bits) and the
    // reference picture list 1 (top 16 bits).
    size_t max_num_ref_frames;
  };

  // EncodeResult owns the necessary resource to keep the encoded buffer. The
  // encoded buffer can be downloaded with the EncodeResult, for example, by
  // calling VaapiWrapper::DownloadFromVABuffer().
  class EncodeResult {
   public:
    EncodeResult(std::unique_ptr<ScopedVABuffer> coded_buffer,
                 const BitstreamBufferMetadata& metadata);
    ~EncodeResult();
    EncodeResult(EncodeResult&&);
    EncodeResult& operator=(EncodeResult&&);
    EncodeResult(const EncodeResult&) = delete;
    EncodeResult& operator=(const EncodeResult&) = delete;

    VABufferID coded_buffer_id() const;
    const BitstreamBufferMetadata& metadata() const;
    bool IsFrameDropped() const { return !coded_buffer_; }

   private:
    std::unique_ptr<ScopedVABuffer> coded_buffer_;
    BitstreamBufferMetadata metadata_;
  };

  // An abstraction of an encode job for one frame. Parameters required for an
  // EncodeJob to be executed are prepared by an VaapiVideoEncoderDelegate,
  // while the accelerator-specific callbacks required to set up and execute it
  // are provided by the accelerator itself, based on these parameters.
  // Accelerators are also responsible for providing any resources (such as
  // memory for output, etc.) as needed.
  class EncodeJob {
   public:
    // Creates an EncodeJob to encode the va surface associated with
    // |input_surface_id|, which will be executed by
    // VaapiVideoEncoderDelegate::Encode().
    // If |keyframe| is true, requests this job to produce a keyframe.
    // |picture| is for a reconstructed frame and the encoded chunk is written
    // into the buffer of |coded_buffer|.
    EncodeJob(bool keyframe,
              base::TimeDelta timestamp,
              uint8_t spatial_index,
              bool end_of_picture,
              VASurfaceID input_surface_id,
              scoped_refptr<CodecPicture> picture,
              std::unique_ptr<ScopedVABuffer> coded_buffer);

    EncodeJob(const EncodeJob&) = delete;
    EncodeJob& operator=(const EncodeJob&) = delete;

    ~EncodeJob();

    // Creates EncodeResult with |metadata|. This passes ownership of the
    // resources owned by EncodeJob and therefore must be called with
    // std::move().
    EncodeResult CreateEncodeResult(const BitstreamBufferMetadata& metadata) &&;

    // Requests this job to produce a keyframe; requesting a keyframe may not
    // always result in one being produced by the encoder (e.g. if it would
    // not fit in the bitrate budget).
    void ProduceKeyframe() { keyframe_ = true; }

    // Returns true if this job has been requested to produce a keyframe.
    bool IsKeyframeRequested() const { return keyframe_; }

    void DropFrame() { coded_buffer_.reset(); }
    bool IsFrameDropped() const { return !coded_buffer_; }

    base::TimeDelta timestamp() const;
    uint8_t spatial_index() const;
    // This is a frame in the top spatial layer.
    bool end_of_picture() const;
    uint8_t spatial_idx() const;

    // VA-API specific methods.
    VABufferID coded_buffer_id() const;
    VASurfaceID input_surface_id() const;
    const scoped_refptr<CodecPicture>& picture() const;
   private:
    // True if this job is to produce a keyframe.
    bool keyframe_;
    // |timestamp_| to be added to the produced encoded chunk.
    const base::TimeDelta timestamp_;
    const uint8_t spatial_index_ = 0;
    const bool end_of_picture_;

    // VA-API specific members.
    // Input surface ID and size for video frame data or scaled data.
    const VASurfaceID input_surface_id_;
    const scoped_refptr<CodecPicture> picture_;
    // Buffer that will contain the output bitstream data for this frame.
    std::unique_ptr<ScopedVABuffer> coded_buffer_;
  };

  VaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                            base::RepeatingClosure error_cb);
  virtual ~VaapiVideoEncoderDelegate();

  // Initializes the encoder with requested parameter set |config| and
  // |ave_config|. Returns false if the requested set of parameters is not
  // supported, true on success.
  virtual bool Initialize(
      const VideoEncodeAccelerator::Config& config,
      const VaapiVideoEncoderDelegate::Config& ave_config) = 0;

  // Updates current framerate and/or bitrate to |framerate| in FPS
  // and the specified video bitrate allocation.
  virtual bool UpdateRates(const VideoBitrateAllocation& bitrate_allocation,
                           uint32_t framerate) = 0;

  // Returns coded size for the input buffers required to encode, in pixels;
  // typically visible size adjusted to match codec alignment requirements.
  virtual gfx::Size GetCodedSize() const = 0;

  // Returns minimum size in bytes for bitstream buffers required to fit output
  // stream buffers produced.
  virtual size_t GetBitstreamBufferSize() const;

  // Returns maximum number of reference frames that may be used by the
  // encoder to encode one frame. The client should be able to provide up to
  // at least this many frames simultaneously for encode to make progress.
  virtual size_t GetMaxNumOfRefFrames() const = 0;

  // Prepares and submits the encode operation to underlying driver for an
  // EncodeJob for one frame and returns true on success.
  bool Encode(EncodeJob& encode_job);

  // Creates and returns the encode result for specified EncodeJob by
  // synchronizing the corresponding encode operation. std::nullopt is returned
  // on failure.
  std::optional<EncodeResult> GetEncodeResult(
      std::unique_ptr<EncodeJob> encode_job);

  // Gets the active spatial layer resolutions for K-SVC encoding, VaapiVEA
  // can get this info from the encoder delegate. Returns empty vector on
  // failure.
  virtual std::vector<gfx::Size> GetSVCLayerResolutions() = 0;

 protected:
  // Friend in order o access PrepareEncodeJobResult declaration.
  friend class H264VaapiVideoEncoderDelegateTest;
  friend class VP9VaapiVideoEncoderDelegateTest;
  friend class VaapiVideoEncodeAcceleratorTest;

  enum class PrepareEncodeJobResult {
    kSuccess,  // Submit the encode job successfully.
    kFail,     // Error happens in submitting the encode job.
    kDrop,     // Encode job is dropped. An returned encoded chunk is empty.
  };

  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  base::RepeatingClosure error_cb_;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  virtual BitstreamBufferMetadata GetMetadata(const EncodeJob& encode_job,
                                              size_t payload_size) = 0;

  // Prepares a new |encode_job| to be executed in Accelerator. Returns
  // kSuccess on success, and kFail on failure.
  virtual PrepareEncodeJobResult PrepareEncodeJob(EncodeJob& encode_job) = 0;

  // Notifies the encoded chunk size in bytes with layers info through
  // BitstreamBufferMetadata to update a bitrate controller in
  // VaapiVideoEncoderDelegate. This should be called only if constant
  // quantization encoding is used, which currently is true for VP8, VP9, H264
  // and AV1.
  virtual void BitrateControlUpdate(
      const BitstreamBufferMetadata& metadata) = 0;
};
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_
