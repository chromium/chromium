// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>

#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/memory/ref_counted_memory.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_provider.h"
#include "media/base/bitrate.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_video_encoder_delegate.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/video/video_encode_accelerator.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}  // namespace base

namespace media {

// A VideoEncodeAccelerator implementation that uses VA-API
// (https://01.org/vaapi) for HW-accelerated video encode.
class MEDIA_GPU_EXPORT VaapiVideoEncodeAccelerator
    : public VideoEncodeAccelerator,
      public base::trace_event::MemoryDumpProvider {
 public:
  VaapiVideoEncodeAccelerator();

  VaapiVideoEncodeAccelerator(const VaapiVideoEncodeAccelerator&) = delete;
  VaapiVideoEncodeAccelerator& operator=(const VaapiVideoEncodeAccelerator&) =
      delete;

  ~VaapiVideoEncodeAccelerator() override;

  // VideoEncodeAccelerator implementation.
  SupportedProfiles GetSupportedProfiles() override;
  bool Initialize(const Config& config,
                  Client* client,

                  std::unique_ptr<MediaLog> media_log) override;
  void Encode(scoped_refptr<VideoFrame> frame, bool force_keyframe) override;
  void UseOutputBitstreamBuffer(BitstreamBuffer buffer) override;
  void RequestEncodingParametersChange(
      const Bitrate& bitrate,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void RequestEncodingParametersChange(
      const VideoBitrateAllocation& bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size) override;
  void Destroy() override;
  void Flush(FlushCallback flush_callback) override;
  bool IsFlushSupported() override;

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

 private:
  friend class VaapiVideoEncodeAcceleratorTest;

  using EncodeJob = VaapiVideoEncoderDelegate::EncodeJob;
  using EncodeResult = VaapiVideoEncoderDelegate::EncodeResult;

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

  // Maximum size is four to support the worst case of a given input of a
  // different resolution than the maximum number of spatial layers (3).
  static constexpr size_t kMaxNumSpatialLayersPlusOne = 3 + 1;
  using InputSurfaceMap = base::small_map<
      std::map<gfx::Size, std::unique_ptr<ScopedVASurface>, SizeComparator>,
      kMaxNumSpatialLayersPlusOne>;
  using EncodeSurfacesMap =
      base::small_map<std::map<gfx::Size,
                               std::vector<std::unique_ptr<ScopedVASurface>>,
                               SizeComparator>,
                      kMaxNumSpatialLayersPlusOne>;
  using EncodeSurfacesCountMap =
      base::small_map<std::map<gfx::Size, size_t, SizeComparator>,
                      kMaxNumSpatialLayersPlusOne>;

  // Holds input frames coming from the client ready to be encoded.
  struct InputFrameRef;

  // Thin wrapper around a ScopedVASurface to furnish it with a ReleaseCB to do
  // something with it upon destruction, e.g. return it to a vector or pool.
  class ScopedVASurfaceWrapper;

  //
  // Tasks for each of the VEA interface calls to be executed on
  // |encoder_task_runner_|.
  //
  void InitializeTask(const Config& config);

  bool AttemptedInitialization() const { return !!client_ptr_factory_; }

  // Enqueues |frame| onto the queue of pending inputs and attempts to continue
  // encoding.
  void EncodeTask(scoped_refptr<VideoFrame> frame, bool force_keyframe);

  // Push |buffer| into |available_bitstream_buffers_|, and attempts to return
  // any pending encoded data in it, if any.
  void UseOutputBitstreamBufferTask(BitstreamBuffer buffer);

  void RequestEncodingParametersChangeTask(
      VideoBitrateAllocation bitrate_allocation,
      uint32_t framerate,
      const std::optional<gfx::Size>& size);

  void DestroyTask();
  void FlushTask(FlushCallback flush_callback);

  // Create input and reconstructed surfaces used in encoding whose sizes are
  // |spatial_layer_resolutions| from GpuMemoryBuffer-based VideoFrame |frame|.
  // The created surfaces for input to an encoder driver are filled into
  // |input_surfaces| and, ones used as reconstructed surfaces by the driver are
  // filled to |reconstructed_surfaces|. This must be called only in native
  // input mode.
  bool CreateSurfacesForGpuMemoryBufferEncoding(
      const VideoFrame& frame,
      const std::vector<gfx::Size>& spatial_layer_resolutions,
      std::vector<std::unique_ptr<ScopedVASurfaceWrapper>>* input_surfaces,
      std::vector<std::unique_ptr<ScopedVASurfaceWrapper>>*
          reconstructed_surfaces);

  // Create input and reconstructed surfaces used in encoding from SharedMemory
  // VideoFrame |frame|. This must be called only in non native input mode.
  bool CreateSurfacesForShmemEncoding(
      const VideoFrame& frame,
      std::unique_ptr<ScopedVASurfaceWrapper>* input_surface,
      std::unique_ptr<ScopedVASurfaceWrapper>* reconstructed_surface);

  // Creates or retrieves a ScopedVASurface compatible with |encode_size|. If
  // there is no available surface and the number of previously allocated
  // surfaces is less than threshold, then it returns a reference to a newly
  // created surface. Returns nullptr if too many surfaces have already been
  // allocated, or if creation fails.
  std::unique_ptr<ScopedVASurfaceWrapper> GetOrCreateReconstructedSurface(
      const gfx::Size& encode_size);

  // Creates or retrieves a ScopedVASurface compatible with |encode_size| and
  // |surface_usage_hints|. Returns nullptr if the surfaces fail to be created
  // successfully.
  std::unique_ptr<ScopedVASurfaceWrapper> GetOrCreateInputSurface(
      VaapiWrapper& vaapi_wrapper,
      const gfx::Size& encode_size,
      const std::vector<VaapiWrapper::SurfaceUsageHint>& surface_usage_hints);

  // Creates |vpp_vaapi_wrapper_| if it hasn't been created.
  scoped_refptr<VaapiWrapper> CreateVppVaapiWrapper();

  // Executes BlitSurface() using |vpp_vaapi_wrapper_| with |source_surface|,
  // |source_visible_rect|. Returns the destination VASurface in BlitSurface()
  // whose size is |encode_size| on success, otherwise nullptr.
  std::unique_ptr<ScopedVASurfaceWrapper> ExecuteBlitSurface(
      const ScopedVASurface* source_surface,
      const gfx::Rect source_visible_rect,
      const gfx::Size& encode_size);

  // Checks if sufficient resources for a new encode job with |frame| as input
  // are available, and if so, claims them by associating them with
  // a EncodeJob, and returns the newly-created job, nullptr otherwise.
  std::unique_ptr<EncodeJob> CreateEncodeJob(
      bool force_keyframe,
      base::TimeDelta frame_timestamp,
      uint8_t spatial_index,
      bool end_of_picture,
      VASurfaceID input_surface_id,
      std::unique_ptr<ScopedVASurfaceWrapper> reconstructed_surface);

  // Continues encoding frames as long as input_queue_ is not empty, and we are
  // able to create new EncodeJobs.
  void EncodePendingInputs();

  // Callback that returns a no longer used ScopedVASurface |va_surface| to
  // |input_surfaces_[encode_size]| or |available_encode_surfaces_[encode_size]|
  // respectively, for reuse.
  void RecycleInputScopedVASurface(const gfx::Size& encode_size,
                                   std::unique_ptr<ScopedVASurface> va_surface);
  void RecycleEncodeScopedVASurface(
      const gfx::Size& encode_size,
      std::unique_ptr<ScopedVASurface> va_surface);

  // Returns pending bitstream buffers to the client if we have both pending
  // encoded data to be completed and bitstream buffers available to download
  // the encoded data into.
  void TryToReturnBitstreamBuffers();

  // Downloads encoded data produced as a result of running |encode_result| into
  // |buffer|, and returns it to the client.
  void ReturnBitstreamBuffer(const EncodeResult& encode_result,
                             const BitstreamBuffer& buffer);

  // Puts the encoder into en error state and notifies the client
  // about the error.
  void NotifyError(EncoderStatus status);

  // Sets the encoder state to |state| on the correct thread.
  void SetState(State state);

  bool IsConfiguredForTesting() const {
    return !supported_profiles_for_testing_.empty();
  }

  // Having too many encoder instances at once may cause us to run out of FDs
  // and subsequently crash (crbug.com/1289465). To avoid that, we limit the
  // maximum number of encoder instances that can exist at once.
  // |num_instances_| tracks that number.
  static constexpr int kMaxNumOfInstances = 10;
  static base::AtomicRefCount num_instances_;
  const bool can_use_encoder_ GUARDED_BY_CONTEXT(child_sequence_checker_);

  // All of the members below must be accessed on the encoder_task_runner_,
  // while it is running.

  // The unchanged values are filled upon the construction. The varied values
  // are filled properly during encoding.
  VideoEncoderInfo encoder_info_ GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // VaapiWrapper is the owner of all HW resources (surfaces and buffers)
  // and will free them on destruction.
  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // The expected coded size of incoming video frames when |native_input_mode_|
  // is false.
  gfx::Size expected_input_coded_size_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // The codec of the stream to be produced. Set during initialization.
  VideoCodec output_codec_ GUARDED_BY_CONTEXT(encoder_sequence_checker_) =
      VideoCodec::kUnknown;

  // The visible rect to be encoded.
  gfx::Rect visible_rect_ GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Size in bytes required for output bitstream buffers.
  size_t output_buffer_byte_size_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_) = 0;
  // Size of the max size of |pending_encode_results_|.
  size_t max_pending_results_size_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_) = 0;

  // This flag signals when the client is sending NV12 + DmaBuf-backed
  // VideoFrames to encode, which allows for skipping a copy-adaptation on
  // input.
  bool native_input_mode_ GUARDED_BY_CONTEXT(encoder_sequence_checker_) = false;

  // The number of frames that needs to be held on encoding.
  size_t num_frames_in_flight_ GUARDED_BY_CONTEXT(encoder_sequence_checker_) =
      0;

  // Encoder state. Encode tasks will only run in kEncoding state.
  State state_ GUARDED_BY_CONTEXT(encoder_sequence_checker_) =
      State::kUninitialized;

  // Encoder instance managing video codec state and preparing encode jobs.
  // Should only be used on |encoder_task_runner_|.
  std::unique_ptr<VaapiVideoEncoderDelegate> encoder_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Map of input surfaces. In non |native_input_mode_|, this is always created
  // and memory-based encode input VideoFrame is written into this.
  // In |native_input_mode_|, this is created only if scaling or cropping is
  // required and used as a VPP destination.
  InputSurfaceMap input_surfaces_ GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Map of available reconstructed surfaces for encoding index by a layer
  // resolution. These are stored as reference frames in
  // VaapiVideoEncoderDelegate if necessary.
  EncodeSurfacesMap available_encode_surfaces_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Map of the number of allocated reconstructed surfaces for encoding
  // indexed by a layer resolution.
  EncodeSurfacesCountMap encode_surfaces_count_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Queue of input frames to be encoded.
  base::queue<InputFrameRef> input_queue_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // BitstreamBuffers mapped, ready to be filled with encoded stream data.
  base::queue<BitstreamBuffer> available_bitstream_buffers_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // VASurfaces already encoded and waiting for the bitstream buffer to
  // be downloaded.
  base::queue<std::optional<EncodeResult>> pending_encode_results_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // Task runner for interacting with the client, and its checker.
  const scoped_refptr<base::SequencedTaskRunner> child_task_runner_;
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
  scoped_refptr<VaapiWrapper> vpp_vaapi_wrapper_
      GUARDED_BY_CONTEXT(encoder_sequence_checker_);

  // The completion callback of the Flush() function.
  FlushCallback flush_callback_ GUARDED_BY_CONTEXT(encoder_sequence_checker_);

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
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_ENCODE_ACCELERATOR_H_
