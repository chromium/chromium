// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "media/base/bitrate.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/video/video_encode_accelerator.h"

namespace media {
class VaapiWrapper;

// A VideoEncodeAccelerator implementation that uses VA-API
// (https://01.org/vaapi) for HW-accelerated video encode.
class MEDIA_GPU_EXPORT VaapiVideoEncodeAccelerator
    : public VideoEncodeAccelerator {
 public:
  VaapiVideoEncodeAccelerator();
  ~VaapiVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config, Client* client) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(const Bitrate& bitrate,
                                       uint32_t framerate) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;

 private:
  friend class VaapiVideoEncodeAcceleratorTest;

  using EncodeJob = VaapiVideoEncoderDelegate::EncodeJob;

  // Encoder state.
  enum State {
    kUninitialized,
    kEncoding,
    kError,
  };

  struct SizeComparator {
    constexpr bool operator()(const gfx::Size& lhs,
                              const gfx::Size& rhs) const {
      return std::forward_as_tuple(lhs.width(), lhs.height()) <
             std::forward_as_tuple(rhs.width(), rhs.height());
    }
  };

  // Holds input frames coming from the client ready to be encoded.
  struct InputFrameRef;
  // Holds output buffers coming from the client ready to be filled.
  struct BitstreamBufferRef;

  // one surface for input data.
  // one surface for reconstructed picture, which is later used for reference.
  static constexpr size_t kNumSurfacesPerInputVideoFrame = 1;
  static constexpr size_t kNumSurfacesForOutputPicture = 1;

  //
  // Tasks for each of the VEA interface calls to be executed on
  // |encoder_task_runner_|.
  //
  void InitializeTask(const Config& config);

  // Enqueues |frame| onto the queue of pending inputs and attempts to continue
  // encoding.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Maps |buffer_ref|, push it onto the available_bitstream_buffers_, and
  // attempts to return any pending encoded data in it, if any.
  void UseOutputBitstreamBufferTask(
      std::unique_ptr<BitstreamBufferRef> buffer_ref);

  void RequestEncodingParametersChangeTask(
      VideoBitrateAllocation bitrate_allocation,
      uint32_t framerate);

  void DestroyTask();
  void FlushTask(FlushCallback flush_callback);

  // Blits |input_surface| to an internally-allocated |input_visible_rect|
  // surface, returning it. If |vpp_vaapi_wrapper_| is empty, this will create
  // it and corresponding surfaces. Returns nullptr on failure.
  scoped_refptr<VASurface> BlitSurfaceWithCreateVppIfNeeded(
      const VASurface& input_surface,
      const gfx::Rect& input_visible_rect,
      const gfx::Size& encode_size,
      size_t num_va_surfaces);

  // Create input and reconstructed surfaces used in encoding whose sizes are
  // |encode_size| from GpuMemoryBuffer-based VideoFrame |frame|. This must be
  // called only in native input mode.
  bool CreateSurfacesForGpuMemoryBufferEncoding(
      const VideoFrame& frame,
      const gfx::Size& encode_size,
      scoped_refptr<VASurface>* input_surface,
      scoped_refptr<VASurface>* reconstructed_surface);

  // Create input and reconstructed surfaces used in encoding from SharedMemory
  // VideoFrame |frame|. This must be called only in non native input mode.
  bool CreateSurfacesForShmemEncoding(
      const VideoFrame& frame,
      scoped_refptr<VASurface>* input_surface,
      scoped_refptr<VASurface>* reconstructed_surface);

  // Checks if sufficient resources for a new encode job with |frame| as input
  // are available, and if so, claims them by associating them with
  // a EncodeJob, and returns the newly-created job, nullptr otherwise.
  std::unique_ptr<EncodeJob> CreateEncodeJob(scoped_refptr<VideoFrame> frame,
                                             bool force_keyframe,
                                             const gfx::Size& encode_size);

  // Continues encoding frames as long as input_queue_ is not empty, and we are
  // able to create new EncodeJobs.
  void EncodePendingInputs();

  // Uploads image data from |frame| to |va_surface_id|.
  void UploadFrame(scoped_refptr<VideoFrame> frame,
                   VASurfaceID va_surface_id,
                   const gfx::Size& va_surface_size);

  // Executes encode in hardware. This does not block and may return before
  // the job is finished.
  void ExecuteEncode(VASurfaceID va_surface_id);

  // Callback that returns a no longer used ScopedVASurface to
  // |va_surfaces| for reuse and kicks EncodePendingInputs() again.
  void RecycleVASurface(
      std::vector<std::unique_ptr<ScopedVASurface>>* va_surfaces,
      std::unique_ptr<ScopedVASurface> va_surface,
      VASurfaceID va_surface_id);

  // Gets available VASurface from |va_surfaces| and returns it as
  // scoped_refptr<VASurface>.
  scoped_refptr<VASurface> GetAvailableVASurfaceAsRefCounted(
      std::vector<std::unique_ptr<ScopedVASurface>>* va_surfaces);

  // Returns a bitstream buffer to the client if both a previously executed job
  // awaits to be completed and we have bitstream buffers available to download
  // the encoded data into.
  void TryToReturnBitstreamBuffer();

  // Downloads encoded data produced as a result of running |encode_job| into
  // |buffer|, and returns it to the client.
  void ReturnBitstreamBuffer(std::unique_ptr<EncodeJob> encode_job,
                             std::unique_ptr<BitstreamBufferRef> buffer);

  // Puts the encoder into en error state and notifies the client
  // about the error.
  void NotifyError(Error error);

  // Sets the encoder state to |state| on the correct thread.
  void SetState(State state);

  bool IsConfiguredForTesting() const {
    return !supported_profiles_for_testing_.empty();
  }

  // The unchanged values are filled upon the construction. The varied values
  // are filled properly during encoding.
  VideoEncoderInfo encoder_info_;

  // VaapiWrapper is the owner of all HW resources (surfaces and buffers)
  // and will free them on destruction.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  // The expected coded size of incoming video frames when |native_input_mode_|
  // is false.
  gfx::Size expected_input_coded_size_;

  // The codec of the stream to be produced. Set during initialization.
  VideoCodec output_codec_ = kUnknownVideoCodec;

  // The visible rect to be encoded.
  gfx::Rect visible_rect_;

  // Size in bytes required for output bitstream buffers.
  size_t output_buffer_byte_size_;

  // This flag signals when the client is sending NV12 + DmaBuf-backed
  // VideoFrames to encode, which allows for skipping a copy-adaptation on
  // input.
  bool native_input_mode_ = false;

  // The number of va surfaces required for one video frame on Encode().
  // In |native_input_mode_|, one surface for input data is created from DmaBufs
  // of incoming VideoFrame. One surface for reconstructed picture is always
  // needed, which is later used for reference.
  // Therefore, |va_surfaces_per_video_frame| is one in |native_input_mode_|,
  // and two otherwise.
  size_t va_surfaces_per_video_frame_;

  // The number of frames that needs to be held on encoding.
  size_t num_frames_in_flight_;

  // All of the members below must be accessed on the encoder_task_runner_,
  // while it is running.

  // Encoder state. Encode tasks will only run in kEncoding state.
  State state_;

  // Encoder instance managing video codec state and preparing encode jobs.
  // Should only be used on |encoder_task_runner_|.
  std::unique_ptr<VaapiVideoEncoderDelegate> encoder_;

  // VA surfaces available for encoding.
  std::vector<std::unique_ptr<ScopedVASurface>> available_va_surfaces_;
  // VA surfaces available for scaling.
  // TODO(crbug.com/1186051): Use base::small_map.
  std::map<gfx::Size,
           std::vector<std::unique_ptr<ScopedVASurface>>,
           SizeComparator>
      available_vpp_va_surfaces_;

  // VA buffers for coded frames.
  std::vector<VABufferID> available_va_buffer_ids_;

  // Queue of input frames to be encoded.
  base::queue<std::unique_ptr<InputFrameRef>> input_queue_;

  // BitstreamBuffers mapped, ready to be filled with encoded stream data.
  base::queue<std::unique_ptr<BitstreamBufferRef>> available_bitstream_buffers_;

  // Jobs submitted to driver for encode, awaiting bitstream buffers to become
  // available.
  base::queue<std::unique_ptr<EncodeJob>> submitted_encode_jobs_;

  // Task runner for interacting with the client, and its checker.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;
  SEQUENCE_CHECKER(child_sequence_checker_);

  // Encoder sequence and its checker. All tasks are executed on it.
  const scoped_refptr<base::SingleThreadTaskRunner> encoder_task_runner_;
  SEQUENCE_CHECKER(encoder_sequence_checker_);

  // To expose client callbacks from VideoEncodeAccelerator.
  // NOTE: all calls to these objects *MUST* be executed on
  // child_task_runner_.
  std::unique_ptr<base::WeakPtrFactory<Client>> client_ptr_factory_;
  base::WeakPtr<Client> client_;

  // VaapiWrapper for VPP (Video Pre Processing). This is used for scale down
  // for the picture send to vaapi encoder.
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_;

  // The completion callback of the Flush() function.
  FlushCallback flush_callback_;

  // Supported profiles that are filled if and only if in a unit test.
  SupportedProfiles supported_profiles_for_testing_;

  // WeakPtr of this, bound to |child_task_runner_|.
  base::WeakPtr<VaapiVideoEncodeAccelerator> child_weak_this_;
  // WeakPtr of this, bound to |encoder_task_runner_|.
  base::WeakPtr<VaapiVideoEncodeAccelerator> encoder_weak_this_;
  base::WeakPtrFactory<VaapiVideoEncodeAccelerator> child_weak_this_factory_{
      this};
  base::WeakPtrFactory<VaapiVideoEncodeAccelerator> encoder_weak_this_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoEncodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
