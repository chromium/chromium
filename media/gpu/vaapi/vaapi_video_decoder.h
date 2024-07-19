// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_

#include <stdint.h>
#include <va/va.h>

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "base/containers/id_map.h"
#include "base/containers/lru_cache.h"
#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_aspect_ratio.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/vaapi/vaapi_decode_surface_handler.h"
#include "media/gpu/vaapi/vaapi_status.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class AcceleratedVideoDecoder;
class VaapiVideoDecoderDelegate;
class DmabufVideoFramePool;
class VaapiWrapper;
class FrameResource;
class VASurface;
class ScopedVASurface;

class VaapiVideoDecoder : public VideoDecoderMixin,
                          public VaapiDecodeSurfaceHandler {
 public:
  static std::unique_ptr<VideoDecoderMixin> Create(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  VaapiVideoDecoder(const VaapiVideoDecoder&) = delete;
  VaapiVideoDecoder& operator=(const VaapiVideoDecoder&) = delete;

  static std::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs();

  // VideoDecoderMixin implementation, VideoDecoder part.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const PipelineOutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  // VideoDecoderMixin implementation, specific part.
  void ApplyResolutionChange() override;
  bool NeedsTranscryption() override;

  // VaapiDecodeSurfaceHandler implementation.
  std::unique_ptr<VASurfaceHandle> CreateSurface() override;
  void SurfaceReady(VASurfaceID va_surface_id,
                    int32_t buffer_id,
                    const gfx::Rect& visible_rect,
                    const VideoColorSpace& color_space) override;

  // Must be called before Initialize().
  void set_ignore_resolution_changes_to_smaller_vp9_for_testing(bool value);
  ~VaapiVideoDecoder() override;

 private:
  friend class VaapiVideoDecoderTest;

  // Decode task holding single decode request.
  struct DecodeTask {
    DecodeTask(scoped_refptr<DecoderBuffer> buffer,
               int32_t buffer_id,
               DecodeCB decode_done_cb);

    DecodeTask(const DecodeTask&) = delete;
    DecodeTask& operator=(const DecodeTask&) = delete;

    DecodeTask(DecodeTask&&);
    DecodeTask& operator=(DecodeTask&&) = default;

    ~DecodeTask();

    scoped_refptr<DecoderBuffer> buffer_;
    int32_t buffer_id_ = -1;
    DecodeCB decode_done_cb_;
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
    kExpectingReset,       // resolution change is aborted, waiting for decoder
                           // to be reset.
    kResetting,            // resetting decoder.
    kError,                // decoder encountered an error.
  };

  VaapiVideoDecoder(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  // Schedule the next decode task in the queue to be executed.
  void ScheduleNextDecodeTask();
  // Try to decode a single input buffer.
  void HandleDecodeTask();
  // Clear the decode task queue. This is done when resetting or destroying the
  // decoder, or encountering an error.
  void ClearDecodeTaskQueue(DecoderStatus status);

  // Releases the local reference to the FrameResource associated with the
  // specified |surface_id| on the decoder thread. This is called when
  // |decoder_| has outputted the FrameResource and stopped using it as a
  // reference frame. Note that this doesn't mean the frame can be reused
  // immediately, as it might still be used by the client.
  void ReleaseVideoFrame(VASurfaceID surface_id);
  // Callback for the frame pool to notify us when a frame becomes available.
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
  VaapiStatus CreateAcceleratedVideoDecoder();

  // Change the current |state_| to the specified |state|.
  void SetState(State state);

  // Tell SetState() to change the |state_| to kError and send |message| to
  // MediaLog and to LOG(ERROR).
  void SetErrorState(std::string message);

  // Callback for the CDM to notify |this|.
  void OnCdmContextEvent(CdmContext::Event event);

  // This is a callback from ApplyResolutionChange() when we need to query the
  // browser process for the screen sizes.
  void ApplyResolutionChangeWithScreenSizes(
      const std::vector<gfx::Size>& screen_resolution);

  // Private static helper to allow using weak ptr instead of an unretained ptr.
  static CroStatus::Or<scoped_refptr<FrameResource>> AllocateCustomFrameProxy(
      base::WeakPtr<VaapiVideoDecoder> decoder,
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      bool use_protected,
      bool use_linear_buffers,
      bool needs_detiling,
      base::TimeDelta timestamp);

  // Allocates a new FrameResource using a new VASurface directly. Since this is
  // only used on linux, it also sets the required YCbCr information for the
  // frame it creates.
  CroStatus::Or<scoped_refptr<FrameResource>> AllocateCustomFrame(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      bool use_protected,
      bool use_linear_buffers,
      bool needs_detiling,
      base::TimeDelta timestamp);

  bool IsConfiguredForTesting() const {
    // Mock instances of |vaapi_wrapper_| and |decoder_| are created and
    // injected to VaapiVideoDecoder for testing purposes.
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !!vaapi_wrapper_ && !!decoder_;
  }

  // The video decoder's state.
  State state_ GUARDED_BY_CONTEXT(sequence_checker_) = State::kUninitialized;

  // Callback used to notify the client when a frame is available for output.
  PipelineOutputCB output_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Callback used to notify the client when we have lost decode context and
  // request a reset (Used in protected decoding).
  WaitingCB waiting_cb_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Bitstream information, written during Initialize().
  VideoCodecProfile profile_ GUARDED_BY_CONTEXT(sequence_checker_) =
      VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoColorSpace color_space_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::optional<gfx::HDRMetadata> hdr_metadata_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Aspect ratio from the config.
  VideoAspectRatio aspect_ratio_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The time at which each buffer decode operation started. Not each decode
  // operation leads to a frame being output and frames might be reordered, so
  // we don't know when it's safe to drop a timestamp. This means we need to use
  // a cache here, with a size large enough to account for frame reordering.
  base::LRUCache<int32_t, base::TimeDelta> buffer_id_to_timestamp_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Queue containing all requested decode tasks.
  base::queue<DecodeTask> decode_task_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // The decode task we're currently trying to execute.
  std::optional<DecodeTask> current_decode_task_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // The next input buffer id.
  int32_t next_buffer_id_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // The list of frames currently used as output buffers or reference frames.
  std::map<VASurfaceID, scoped_refptr<FrameResource>> output_frames_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // ScopedVASurfaces are created by importing resources from a
  // DmabufVideoFramePool into libva via CreateSurface(). The following map
  // keeps those ScopedVASurfaces for reuse according to the expectations of
  // libva vaDestroySurfaces(): "Surfaces can only be destroyed after all
  // contexts using these surfaces have been destroyed."
  // TODO(crbug.com/1040291): remove this keep-alive when using SharedImages.
  base::IDMap<std::unique_ptr<ScopedVASurface>,
              decltype(gfx::GpuMemoryBufferId::id)>
      allocated_va_surfaces_ GUARDED_BY_CONTEXT(sequence_checker_);

  // We need to use a CdmContextRef so that we destruct
  // |cdm_event_cb_registration_| before the CDM is destructed. The CDM has
  // mechanisms to ensure destruction on the proper thread.
  //
  // For clarity, the MojoVideoDecoderService does hold a reference to both the
  // decoder and the CDM to ensure the CDM doesn't get destructed before the
  // decoder; however, in the VideoDecoderPipeline, which owns the
  // VaapiVideoDecoder, it uses an asynchronous destructor to destroy the
  // pipeline (and thus the VaapiVideoDecoder) on the decoder thread.
  std::unique_ptr<CdmContextRef> cdm_context_ref_
      GUARDED_BY_CONTEXT(sequence_checker_);

  EncryptionScheme encryption_scheme_ GUARDED_BY_CONTEXT(sequence_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // To keep the CdmContext event callback registered.
  std::unique_ptr<CallbackRegistration> cdm_event_cb_registration_
      GUARDED_BY_CONTEXT(sequence_checker_);
#endif

  // Platform and codec specific video decoder.
  std::unique_ptr<AcceleratedVideoDecoder> decoder_
      GUARDED_BY_CONTEXT(sequence_checker_);
  scoped_refptr<VaapiWrapper> vaapi_wrapper_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // TODO(crbug.com/1022246): Instead of having the raw pointer here, getting
  // the pointer from AcceleratedVideoDecoder.
  //
  // Dangling in VideoDecoderTest.* on chromeos-amd64-generic-rel-gtest
  raw_ptr<VaapiVideoDecoderDelegate, DanglingUntriaged> decoder_delegate_
      GUARDED_BY_CONTEXT(sequence_checker_) = nullptr;

  // This is used on AMD protected content implementations to indicate that the
  // DecoderBuffers we receive have been transcrypted and need special handling.
  bool transcryption_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // See VP9Decoder for information on this.
  bool ignore_resolution_changes_to_smaller_for_testing_
      GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtr of *|this| and its factory, bound to |decoder_task_runner_|.
  base::WeakPtr<VaapiVideoDecoder> weak_this_;
  base::WeakPtrFactory<VaapiVideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_H_
