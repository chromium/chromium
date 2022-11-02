// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/cdm_context.h"
#include "media/base/limits.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/chromeos_status.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_with_pool.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "media/gpu/chromeos/decoder_buffer_transcryptor.h"
#endif  // BUILDFLAG(IS_CHROMEOS)
namespace base {
class SequencedTaskRunner;
}

namespace media {

class DmabufVideoFramePool;
class MediaLog;

// This interface extends VideoDecoder to provide a few more methods useful
// for VideoDecoderPipeline operation. This class should be operated and
// destroyed on |decoder_task_runner_|.
class MEDIA_GPU_EXPORT VideoDecoderMixin : public VideoDecoder {
 public:
  // Client interface of VideoDecoderMixin.
  class MEDIA_GPU_EXPORT Client {
   public:
    Client() = default;
    virtual ~Client() = default;

    // Returns the video frame pool without giving up ownership or nullptr if
    // the decoder is responsible for allocating its own frames. Note that
    // callers may not assume that the returned pointer is valid after a call to
    // PickDecoderOutputFormat().
    virtual DmabufVideoFramePool* GetVideoFramePool() const = 0;

    // After this method is called from |decoder_|, the client needs to call
    // VideoDecoderMixin::ApplyResolutionChange() when all pending frames are
    // flushed.
    virtual void PrepareChangeResolution() = 0;

    // Negotiates the output format and size of the decoder: if not scaling
    // (i.e., the size of |decoder_visible_rect| is equal to |output_size|), it
    // selects a renderable format out of |candidates| and initializes the main
    // video frame pool with the selected format and the given arguments. If
    // scaling, or if none of the |candidates| is considered renderable, or if
    // (VA-API-only) the modifier of the frame pool's buffers is incompatible
    // with the hardware decoder, this method attempts to initialize an image
    // processor to reconcile the formats and/or perform scaling.
    // |need_aux_frame_pool| indicates whether the caller needs a frame pool in
    // the event that an image processor is needed: if true, a new pool is
    // initialized and that pool can be obtained by calling GetVideoFramePool().
    // This pool will provide buffers consistent with the selected candidate out
    // of |candidates|. If false, the caller must allocate its own buffers.
    // if |allocator| is not absl::nullopt, the frame pool will be set
    // to use the allocator provided for allocating video frames.
    //
    // The client also provides the |num_codec_reference_frames|, which is
    // sometimes fixed (e.g. kVp9NumRefFrames) and sometimes variable (e.g.
    // H.264, HEVC).
    //
    // Note: after a call to this method, callers should assume that a pointer
    // returned by a prior call to GetVideoFramePool() is no longer valid.
    virtual CroStatus::Or<ImageProcessor::PixelLayoutCandidate>
    PickDecoderOutputFormat(
        const std::vector<ImageProcessor::PixelLayoutCandidate>& candidates,
        const gfx::Rect& decoder_visible_rect,
        const gfx::Size& decoder_natural_size,
        absl::optional<gfx::Size> output_size,
        size_t num_codec_reference_frames,
        bool use_protected,
        bool need_aux_frame_pool,
        absl::optional<DmabufVideoFramePool::CreateFrameCB> allocator) = 0;
  };

  VideoDecoderMixin(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);

  VideoDecoderMixin(const VideoDecoderMixin&) = delete;
  VideoDecoderMixin& operator=(const VideoDecoderMixin&) = delete;

  ~VideoDecoderMixin() override;

  // After DecoderInterface calls |prepare_change_resolution_cb| passed
  // from the constructor, this method is called when the pipeline flushes
  // pending frames.
  virtual void ApplyResolutionChange() = 0;

  // For protected content implementations that require transcryption of the
  // content before being sent into the HW decoders. (Currently only used by
  // AMD). Default implementation returns false.
  virtual bool NeedsTranscryption();

  // Set the DMA coherency of the video decoder buffers. Only relevant for
  // V4L2.
  virtual void SetDmaIncoherentV4L2(bool incoherent) {}

 protected:
  const std::unique_ptr<MediaLog> media_log_;

  // Decoder task runner. All public methods of
  // VideoDecoderMixin are executed at this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // The WeakPtr client instance, bound to |decoder_task_runner_|.
  base::WeakPtr<VideoDecoderMixin::Client> client_;
};

class MEDIA_GPU_EXPORT VideoDecoderPipeline : public VideoDecoder,
                                              public VideoDecoderMixin::Client {
 public:
  using CreateDecoderFunctionCB =
      base::OnceCallback<std::unique_ptr<VideoDecoderMixin>(
          std::unique_ptr<MediaLog> media_log,
          scoped_refptr<base::SequencedTaskRunner>,
          base::WeakPtr<VideoDecoderMixin::Client>)>;

  // Creates a VideoDecoderPipeline instance that allocates VideoFrames from
  // |frame_pool| and converts the decoded VideoFrames using |frame_converter|.
  static std::unique_ptr<VideoDecoder> Create(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder);

  static absl::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs(
      const gpu::GpuDriverBugWorkarounds& workarounds);

  ~VideoDecoderPipeline() override;
  static void DestroyAsync(std::unique_ptr<VideoDecoderPipeline>);

  // VideoDecoder implementation
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  int GetMaxDecodeRequests() const override;
  bool FramesHoldExternalResources() const override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;

  // VideoDecoderMixin::Client implementation.
  DmabufVideoFramePool* GetVideoFramePool() const override;
  void PrepareChangeResolution() override;
  CroStatus::Or<ImageProcessor::PixelLayoutCandidate> PickDecoderOutputFormat(
      const std::vector<ImageProcessor::PixelLayoutCandidate>& candidates,
      const gfx::Rect& decoder_visible_rect,
      const gfx::Size& decoder_natural_size,
      absl::optional<gfx::Size> output_size,
      size_t num_codec_reference_frames,
      bool use_protected,
      bool need_aux_frame_pool,
      absl::optional<DmabufVideoFramePool::CreateFrameCB> allocator) override;

 private:
  friend class VideoDecoderPipelineTest;
#if BUILDFLAG(USE_VAAPI)
  FRIEND_TEST_ALL_PREFIXES(VideoDecoderPipelineTest,
                           PickDecoderOutputFormatLinearModifier);
#endif

  VideoDecoderPipeline(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log,
      CreateDecoderFunctionCB create_decoder_function_cb);

  void InitializeTask(const VideoDecoderConfig& config,
                      bool low_delay,
                      CdmContext* cdm_context,
                      InitCB init_cb,
                      const OutputCB& output_cb,
                      const WaitingCB& waiting_cb);
  void ResetTask(base::OnceClosure reset_cb);
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb);

  void OnInitializeDone(InitCB init_cb,
                        CdmContext* cdm_context,
                        DecoderStatus status);

  void OnDecodeDone(bool eos_buffer, DecodeCB decode_cb, DecoderStatus status);
  void OnResetDone(base::OnceClosure reset_cb);
  void OnError(const std::string& msg);

  // Called when |decoder_| finishes decoding a frame.
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame);
  // Called when |image_processor_| finishes processing a frame.
  void OnFrameProcessed(scoped_refptr<VideoFrame> frame);
  // Called when |frame_converter_| finishes converting a frame.
  void OnFrameConverted(scoped_refptr<VideoFrame> frame);
  // Called when |decoder_| invokes the waiting callback.
  void OnDecoderWaiting(WaitingReason reason);

  // Return true if the pipeline has pending frames that are returned from
  // |decoder_| but haven't been passed to the client.
  // i.e. |image_processor_| or |frame_converter_| has pending frames.
  bool HasPendingFrames() const;

  // Call VideoDecoderMixin::ApplyResolutionChange() when we need to.
  void CallApplyResolutionChangeIfNeeded();

  // Call |client_flush_cb_| with |status|.
  void CallFlushCbIfNeeded(DecoderStatus status);

#if BUILDFLAG(IS_CHROMEOS)
  // Callback for when transcryption of a buffer completes.
  void OnBufferTranscrypted(scoped_refptr<DecoderBuffer> transcrypted_buffer,
                            DecodeCB decode_callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Used to figure out the supported configurations in Initialize().
  const gpu::GpuDriverBugWorkarounds gpu_workarounds_;

  SupportedVideoDecoderConfigs supported_configs_for_testing_;

  // The client task runner and its sequence checker. All public methods should
  // run on this task runner.
  const scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  SEQUENCE_CHECKER(client_sequence_checker_);

  // The decoder task runner and its sequence checker. Call |decoder_|'s,
  // |frame_pool_|'s, and |frame_converter_|'s methods on this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;
  SEQUENCE_CHECKER(decoder_sequence_checker_);

  // The frame pool passed from the client. While internally other additional
  // frame pools might be used for intermediate results, all frames passed to
  // the client should be created using this pool. DmabufVideoFramePool is
  // thread safe and used from |client_task_runner_| and
  // |decoder_task_runner_|.
  std::unique_ptr<DmabufVideoFramePool> main_frame_pool_;

  // When an image processor is needed, |auxiliary_frame_pool_| is the pool of
  // output buffers for the |decoder_| (which will serve as the input buffers
  // for the image processor) and |main_frame_pool_| will be the pool of output
  // buffers for the image processor.
  std::unique_ptr<DmabufVideoFramePool> auxiliary_frame_pool_;

  // The image processor is only created when the decoder cannot output frames
  // with renderable format.
  std::unique_ptr<ImageProcessorWithPool> image_processor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // The frame converter passed from the client, otherwise used and destroyed on
  // |decoder_task_runner_|.
  std::unique_ptr<VideoFrameConverter> frame_converter_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  const std::unique_ptr<MediaLog> media_log_;

#if BUILDFLAG(IS_CHROMEOS)
  // The transcryptor for transcrypting DecoderBuffers when needed by the HW
  // decoder implementation.
  std::unique_ptr<DecoderBufferTranscryptor> buffer_transcryptor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // The current video decoder implementation. Valid after initialization is
  // successfully done.
  std::unique_ptr<VideoDecoderMixin> decoder_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Only used after initialization on |decoder_task_runner_|.
  CreateDecoderFunctionCB create_decoder_function_cb_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Callbacks provided by the client. Used on |decoder_task_runner_|.
  // The callback methods themselves are exercised on |client_task_runner_|.
  OutputCB client_output_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  DecodeCB client_flush_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  WaitingCB waiting_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  using CreateImageProcessorCBForTesting =
      base::RepeatingCallback<std::unique_ptr<ImageProcessor>(
          const std::vector<ImageProcessor::PixelLayoutCandidate>&
              input_candidates,
          const gfx::Rect& input_visible_rect,
          const gfx::Size& output_size,
          size_t num_buffers)>;
  CreateImageProcessorCBForTesting create_image_processor_cb_for_testing_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // True if we need to notify |decoder_| that the pipeline is flushed via
  // VideoDecoderMixin::ApplyResolutionChange().
  bool need_apply_new_resolution GUARDED_BY_CONTEXT(decoder_sequence_checker_) =
      false;

  // True if the decoder needs bitstream conversion before decoding.
  bool needs_bitstream_conversion_
      GUARDED_BY_CONTEXT(client_sequence_checker_) = false;

  // Set to true when any unexpected error occurs.
  bool has_error_ GUARDED_BY_CONTEXT(decoder_sequence_checker_) = false;

  // Set to true when we need to tell the frame pool to rebuild itself. This is
  // needed for protected content on Intel platforms.
  bool need_frame_pool_rebuild_ GUARDED_BY_CONTEXT(decoder_sequence_checker_) =
      false;

  // Calculated upon Initialize(), to determine how many buffers to allocate for
  // the Renderer pipeline.
  size_t estimated_num_buffers_for_renderer_ GUARDED_BY_CONTEXT(
      decoder_sequence_checker_) = limits::kMaxVideoFrames + 1;

  // Set to true to bypass checks for encrypted content support for testing.
  bool allow_encrypted_content_for_testing_ = false;

  base::WeakPtr<VideoDecoderPipeline> decoder_weak_this_;
  // The weak pointer of this, bound to |decoder_task_runner_|.
  base::WeakPtrFactory<VideoDecoderPipeline> decoder_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
