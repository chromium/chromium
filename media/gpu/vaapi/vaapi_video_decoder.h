// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_

#include <stdint.h>
#include <va/va.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/mru_cache.h"
#include "base/containers/queue.h"
#include "base/containers/small_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class GpuDriverBugWorkarounds;
}

namespace media {

class AcceleratedVideoDecoder;
class VaapiVideoDecoderDelegate;
class DmabufVideoFramePool;
class VaapiWrapper;
class VideoFrame;
class VASurface;

class VaapiVideoDecoder : public DecoderInterface,
                          public DecodeSurfaceHandler<VASurface> {
 public:
  static std::unique_ptr<DecoderInterface> Create(
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<DecoderInterface::Client> client);

  static SupportedVideoDecoderConfigs GetSupportedConfigs(
      const gpu::GpuDriverBugWorkarounds& workarounds);

  // DecoderInterface implementation.
  void Initialize(const VideoDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  void ApplyResolutionChange() override;

  // DecodeSurfaceHandler<VASurface> implementation.
  scoped_refptr<VASurface> CreateSurface() override;
  scoped_refptr<VASurface> CreateDecodeSurface() override;
  bool IsScalingDecode() override;
  const gfx::Rect GetOutputVisibleRect(
      const gfx::Rect& decode_visible_rect,
      const gfx::Size& output_picture_size) override;
  void SurfaceReady(scoped_refptr<VASurface> va_surface,
                    int32_t buffer_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

 private:
  // Decode task holding single decode request.
  struct DecodeTask {
    DecodeTask(scoped_refptr<DecoderBuffer> buffer,
               int32_t buffer_id,
               DecodeCB decode_done_cb);
    ~DecodeTask();
    DecodeTask(DecodeTask&&);
    DecodeTask& operator=(DecodeTask&&) = default;
    scoped_refptr<DecoderBuffer> buffer_;
    int32_t buffer_id_ = -1;
    DecodeCB decode_done_cb_;
    DISALLOW_COPY_AND_ASSIGN(DecodeTask);
  };

  enum class State {
    kUninitialized,        // not initialized yet or initialization failed.
    kWaitingForInput,      // waiting for input buffers.
    kWaitingForOutput,     // waiting for output buffers.
    kWaitingForProtected,  // waiting on something related to protected content,
                           // either setup, full sample parsing or key loading.
    kDecoding,             // decoding buffers.
    kChangingResolution,   // need to change resolution, waiting for pipeline to
                           // be flushed.
    kResetting,            // resetting decoder.
    kError,                // decoder encountered an error.
  };

  VaapiVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<DecoderInterface::Client> client);
  ~VaapiVideoDecoder() override;

  // Schedule the next decode task in the queue to be executed.
  void ScheduleNextDecodeTask();
  // Try to decode a single input buffer.
  void HandleDecodeTask();
  // Clear the decode task queue. This is done when resetting or destroying the
  // decoder, or encountering an error.
  void ClearDecodeTaskQueue(DecodeStatus status);

  // Releases the local reference to the VideoFrame associated with the
  // specified |surface_id| on the decoder thread. This is called when
  // |decoder_| has outputted the VideoFrame and stopped using it as a
  // reference frame. Note that this doesn't mean the frame can be reused
  // immediately, as it might still be used by the client.
  void ReleaseVideoFrame(VASurfaceID surface_id);
  // Callback for |frame_pool_| to notify of available resources.
  void NotifyFrameAvailable();
  // Callback from accelerator to indicate the protected state has been updated
  // so we can proceed or fail.
  void ProtectedSessionUpdate(bool success);

  // Flushes |decoder_|, blocking until all pending decode tasks have been
  // executed and all frames have been output.
  void Flush();

  // Called when resetting the decoder is finished, to execute |reset_cb|.
  void ResetDone(base::OnceClosure reset_cb);

  // Create codec-specific AcceleratedVideoDecoder and reset related variables.
  Status CreateAcceleratedVideoDecoder();

  // Change the current |state_| to the specified |state|.
  void SetState(State state);

  // Callback for the CDM to notify |this|.
  void OnCdmContextEvent(CdmContext::Event event);

  // This is a callback from ApplyResolutionChange() when we need to query the
  // browser process for the screen sizes.
  void ApplyResolutionChangeWithScreenSizes(
      const std::vector<gfx::Size>& screen_resolution);

  // Callback for when a VASurface in the decode pool is no longer used as a
  // reference frame and should then be returned to the pool. We ignore the
  // VASurfaceID in the normal callback because it is retained in the |surface|
  // object.
  void ReturnDecodeSurfaceToPool(std::unique_ptr<ScopedVASurface> surface,
                                 VASurfaceID);

  // The video decoder's state.
  State state_ = State::kUninitialized;

  // Callback used to notify the client when a frame is available for output.
  OutputCB output_cb_;

  // Callback used to notify the client when we have lost decode context and
  // request a reset. (Used in protected decoding).
  WaitingCB waiting_cb_;

  // The video stream's profile.
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  // Color space of the video frame.
  VideoColorSpace color_space_;

  // Ratio of natural size to |visible_rect_| of the output frames.
  double pixel_aspect_ratio_ = 0.0;

  // Video frame pool used to allocate and recycle video frames.
  DmabufVideoFramePool* frame_pool_ = nullptr;

  // The time at which each buffer decode operation started. Not each decode
  // operation leads to a frame being output and frames might be reordered, so
  // we don't know when it's safe to drop a timestamp. This means we need to use
  // a cache here, with a size large enough to account for frame reordering.
  base::MRUCache<int32_t, base::TimeDelta> buffer_id_to_timestamp_;

  // Queue containing all requested decode tasks.
  base::queue<DecodeTask> decode_task_queue_;
  // The decode task we're currently trying to execute.
  base::Optional<DecodeTask> current_decode_task_;
  // The next input buffer id.
  int32_t next_buffer_id_ = 0;

  // The list of frames currently used as output buffers or reference frames.
  std::map<VASurfaceID, scoped_refptr<VideoFrame>> output_frames_;

  // VASurfaces are created via importing |frame_pool_| resources into libva in
  // CreateSurface(). The following map keeps those VASurfaces for reuse
  // according to the expectations of libva vaDestroySurfaces(): "Surfaces can
  // only be destroyed after all contexts using these surfaces have been
  // destroyed."
  // TODO(crbug.com/1040291): remove this keep-alive when using SharedImages.
  base::small_map<std::map<gfx::GpuMemoryBufferId, scoped_refptr<VASurface>>>
      allocated_va_surfaces_;

  // We need to use a CdmContextRef so that we destruct
  // |cdm_event_cb_registration_| before the CDM is destructed. The CDM has
  // mechanisms to ensure destruction on the proper thread.
  //
  // For clarity, the MojoVideoDecoderService does hold a reference to both the
  // decoder and the CDM to ensure the CDM doesn't get destructed before the
  // decoder; however, in the VideoDecoderPipeline, which owns the
  // VaapiVideoDecoder, it uses an asynchronous destructor to destroy the
  // pipeline (and thus the VaapiVideoDecoder) on the decoder thread.
  std::unique_ptr<CdmContextRef> cdm_context_ref_;

  EncryptionScheme encryption_scheme_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // To keep the CdmContext event callback registered.
  std::unique_ptr<CallbackRegistration> cdm_event_cb_registration_;
#endif

  // Platform and codec specific video decoder.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;
  // TODO(crbug.com/1022246): Instead of having the raw pointer here, getting
  // the pointer from AcceleratedVideoDecoder.
  VaapiVideoDecoderDelegate* decoder_delegate_ = nullptr;

  // When we are doing scaled decoding, this is the pool of surfaces used by the
  // decoder for reference frames.
  base::queue<std::unique_ptr<ScopedVASurface>>
      decode_surface_pool_for_scaling_;

  // When we are doing scaled decoding, this is the scale factor we are using,
  // and applies the same in both dimensions.
  base::Optional<float> decode_to_output_scale_factor_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<VaapiVideoDecoder> weak_this_;
  base::WeakPtrFactory<VaapiVideoDecoder> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(VaapiVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
