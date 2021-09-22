// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_

#include <va/va.h>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/video_bitrate_allocation.h"
#include "media/base/video_codecs.h"
#include "media/video/video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class RefCountedBytes;
}

namespace media {
struct BitstreamBufferMetadata;
class CodecPicture;
class ScopedVABuffer;
class VideoFrame;
class VASurface;
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
  VaapiVideoEncoderDelegate(scoped_refptr<VaapiWrapper> vaapi_wrapper,
                            base::RepeatingClosure error_cb);
  virtual ~VaapiVideoEncoderDelegate();

  enum class BitrateControl {
    kConstantBitrate,  // Constant Bitrate mode. This class relies on other
                       // parts (e.g. driver) to achieve the specified bitrate.
    kConstantQuantizationParameter  // Constant Quantization Parameter mode.
                                    // This class needs to compute a proper
                                    // quantization parameter and give other
                                    // parts (e.g. the driver) the value.
  };

  struct Config {
    // Maxium number of reference frames.
    // For H.264 encoding, the value represents the maximum number of reference
    // frames for both the reference picture list 0 (bottom 16 bits) and the
    // reference picture list 1 (top 16 bits).
    size_t max_num_ref_frames;

    BitrateControl bitrate_control = BitrateControl::kConstantBitrate;
  };

  // An abstraction of an encode job for one frame. Parameters required for an
  // EncodeJob to be executed are prepared by an VaapiVideoEncoderDelegate,
  // while the accelerator-specific callbacks required to set up and execute it
  // are provided by the accelerator itself, based on these parameters.
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
    // Constructor for VA-API.
    EncodeJob(scoped_refptr<VideoFrame> input_frame,
              bool keyframe,
              base::OnceClosure execute_cb,
              scoped_refptr<VASurface> input_surface,
              scoped_refptr<CodecPicture> picture,
              std::unique_ptr<ScopedVABuffer> coded_buffer);

    EncodeJob(const EncodeJob&) = delete;
    EncodeJob& operator=(const EncodeJob&) = delete;

    ~EncodeJob();

    // Schedules a callback to be run immediately before this job is executed.
    // Can be called multiple times to schedule multiple callbacks, and all
    // of them will be run, in order added.
    // Callbacks can be used to e.g. set up hardware parameters before the job
    // is executed.
    void AddSetupCallback(base::OnceClosure cb);

    // Schedules a callback to be run immediately after this job is executed.
    // Can be called multiple times to schedule multiple callbacks, and all
    // of them will be run, in order added. Callbacks can be used to e.g. get
    // the encoded buffer linear size.
    void AddPostExecuteCallback(base::OnceClosure cb);

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

    // VA-API specific methods.
    VABufferID coded_buffer_id() const;
    const scoped_refptr<VASurface>& input_surface() const;
    const scoped_refptr<CodecPicture>& picture() const;

   private:
    // Input VideoFrame to be encoded.
    const scoped_refptr<VideoFrame> input_frame_;

    // Source timestamp for |input_frame_|.
    const base::TimeDelta timestamp_;

    // True if this job is to produce a keyframe.
    bool keyframe_;

    // VA-API specific members.
    // Input surface for video frame data or scaled data.
    const scoped_refptr<VASurface> input_surface_;
    const scoped_refptr<CodecPicture> picture_;
    // Buffer that will contain the output bitstream data for this frame.
    const std::unique_ptr<ScopedVABuffer> coded_buffer_;

    // Callbacks to be run (in the same order as the order of AddSetupCallback()
    // calls) to set up the job.
    base::queue<base::OnceClosure> setup_callbacks_;

    // Callbacks to be run (in the same order as the order of
    // AddPostExecuteCallback() calls) to do post processing after execute.
    base::queue<base::OnceClosure> post_execute_callbacks_;

    // Callback to be run to execute this job.
    base::OnceClosure execute_callback_;

    // Reference pictures required for this job.
    std::vector<scoped_refptr<CodecPicture>> reference_pictures_;
  };

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

  // Prepares a new |encode_job| to be executed in Accelerator and returns true
  // on success. The caller may then call Execute() on the job to run it.
  virtual bool PrepareEncodeJob(EncodeJob* encode_job) = 0;

  // Notifies the encoded chunk size in bytes to update a bitrate controller in
  // VaapiVideoEncoderDelegate. This should be called only if
  // VaapiVideoEncoderDelegate is configured with
  // BitrateControl::kConstantQuantizationParameter.
  virtual void BitrateControlUpdate(uint64_t encoded_chunk_size_bytes);

  virtual BitstreamBufferMetadata GetMetadata(EncodeJob* encode_job,
                                              size_t payload_size);

  // Gets the active spatial layer resolutions for K-SVC encoding, VaapiVEA
  // can get this info from the encoder delegate. Returns empty vector on
  // failure.
  virtual std::vector<gfx::Size> GetSVCLayerResolutions() = 0;

  // Submits |buffer| of |type| to the driver.
  void SubmitBuffer(VABufferType type,
                    scoped_refptr<base::RefCountedBytes> buffer);

  // Submits a VAEncMiscParameterBuffer |buffer| of type |type| to the driver.
  void SubmitVAEncMiscParamBuffer(VAEncMiscParameterType type,
                                  scoped_refptr<base::RefCountedBytes> buffer);

 protected:
  const scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  base::RepeatingClosure error_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODER_DELEGATE_H_
