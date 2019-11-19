// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_ACCELERATED_VIDEO_ENCODER_H_
#define MEDIA_GPU_VAAPI_ACCELERATED_VIDEO_ENCODER_H_

#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/gpu/codec_picture.h"
#include "media/video/video_encode_accelerator.h"
#include "ui/gfx/geometry/size.h"

namespace media {

struct BitstreamBufferMetadata;
class VaapiEncodeJob;
class VideoFrame;

// An AcceleratedVideoEncoder (AVE) performs high-level, platform-independent
// encoding process tasks, such as managing codec state, reference frames, etc.,
// but may require support from an external accelerator (typically a hardware
// accelerator) to offload some stages of the actual encoding process, using
// the parameters that AVE prepares beforehand.
//
// For each frame to be encoded, clients provide an EncodeJob object to be set
// up by an AVE with job parameters, and execute the job afterwards. Any
// resources required for the job are also provided by the clients, and
// associated with the EncodeJob object.
class AcceleratedVideoEncoder {
 public:
  AcceleratedVideoEncoder() = default;
  virtual ~AcceleratedVideoEncoder() = default;

  struct Config {
    // Maxium number of reference frames.
    // For H.264 encoding, the value represents the maximum number of reference
    // frames for both the reference picture list 0 (bottom 16 bits) and the
    // reference picture list 1 (top 16 bits).
    size_t max_num_ref_frames;
  };

  // An abstraction of an encode job for one frame. Parameters required for an
  // EncodeJob to be executed are prepared by an AcceleratedVideoEncoder, while
  // the accelerator-specific callbacks required to set up and execute it are
  // provided by the accelerator itself, based on these parameters.
  // Accelerators are also responsible for providing any resources (such as
  // memory for output and reference pictures, etc.) as needed.
  class EncodeJob {
   public:
    // Creates an EncodeJob to encode |input_frame|, which will be executed
    // by calling |execute_cb|. If |keyframe| is true, requests this job
    // to produce a keyframe.
    EncodeJob(scoped_refptr<VideoFrame> input_frame,
              bool keyframe,
              base::OnceClosure execute_cb);
    virtual ~EncodeJob();

    // Schedules a callback to be run immediately before this job is executed.
    // Can be called multiple times to schedule multiple callbacks, and all
    // of them will be run, in order added.
    // Callbacks can be used to e.g. set up hardware parameters before the job
    // is executed.
    void AddSetupCallback(base::OnceClosure cb);

    // Adds |ref_pic| to the list of pictures to be used as reference pictures
    // for this frame, to ensure they remain valid until the job is executed
    // (or discarded).
    void AddReferencePicture(scoped_refptr<CodecPicture> ref_pic);

    // Runs all setup callbacks previously scheduled, if any, in order added,
    // and executes the job by calling the execute callback. Note that the
    // actual job execution may be asynchronous, and returning from this method
    // does not have to indicate that the job has been finished. The execute
    // callback is responsible for retaining references to any resources that
    // may be in use after this method returns however, so it is safe to release
    // the EncodeJob object itself immediately after this method returns.
    void Execute();

    // Requests this job to produce a keyframe; requesting a keyframe may not
    // always result in one being produced by the encoder (e.g. if it would
    // not fit in the bitrate budget).
    void ProduceKeyframe() { keyframe_ = true; }

    // Returns true if this job has been requested to produce a keyframe.
    bool IsKeyframeRequested() const { return keyframe_; }

    // Returns the timestamp associated with this job.
    base::TimeDelta timestamp() const { return timestamp_; }

    virtual BitstreamBufferMetadata Metadata(size_t payload_size) const;

    virtual VaapiEncodeJob* AsVaapiEncodeJob();

   private:
    // Input VideoFrame to be encoded.
    const scoped_refptr<VideoFrame> input_frame_;

    // Source timestamp for |input_frame_|.
    const base::TimeDelta timestamp_;

    // True if this job is to produce a keyframe.
    bool keyframe_;

    // Callbacks to be run (in the same order as the order of AddSetupCallback()
    // calls) to set up the job.
    base::queue<base::OnceClosure> setup_callbacks_;

    // Callback to be run to execute this job.
    base::OnceClosure execute_callback_;

    // Reference pictures required for this job.
    std::vector<scoped_refptr<CodecPicture>> reference_pictures_;

    DISALLOW_COPY_AND_ASSIGN(EncodeJob);
  };

  // Initializes the encoder with requested parameter set |config| and
  // |ave_config|. Returns false if the requested set of parameters is not
  // supported, true on success.
  virtual bool Initialize(
      const VideoEncodeAccelerator::Config& config,
      const AcceleratedVideoEncoder::Config& ave_config) = 0;

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

  // Prepares a new |encode_job| to be executed in Accelerator and returns true
  // on success. The caller may then call Execute() on the job to run it.
  virtual bool PrepareEncodeJob(EncodeJob* encode_job) = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_ACCELERATED_VIDEO_ENCODER_H_
