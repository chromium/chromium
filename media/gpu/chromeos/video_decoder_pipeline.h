// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_

#include <atomic>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
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
#include "media/gpu/chromeos/frame_resource.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "media/gpu/chromeos/image_processor_with_pool.h"
#include "media/gpu/media_gpu_export.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
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
  // Used by decoders to send FrameResource objects to the decoder pipeline for
  // conversion, image processing, etc.
  using PipelineOutputCB =
      base::RepeatingCallback<void(scoped_refptr<FrameResource>)>;

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

    // Notify of the estimated maximum possible number of DecodeRequests. This
    // method can be called from any thread and as many times as desired.
    virtual void NotifyEstimatedMaxDecodeRequests(int num) = 0;

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
    // if |allocator| is not std::nullopt, the frame pool will be set
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
        std::optional<gfx::Size> output_size,
        size_t num_codec_reference_frames,
        bool use_protected,
        bool need_aux_frame_pool,
        std::optional<DmabufVideoFramePool::CreateFrameCB> allocator) = 0;
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
  // content before being sent into the HW decoders. (Currently used by AMD and
  // ARM). Default implementation returns false.
  virtual bool NeedsTranscryption();

  // For protected content implementations that decrypt to a secure memory
  // buffer (i.e. TrustZone on ARM), this is used to attach the appropriate
  // handle for the secure buffer to the DecoderBuffer.
  virtual CroStatus AttachSecureBuffer(scoped_refptr<DecoderBuffer>& buffer);
  // Counterpart to AttachSecureBuffer, this should be invoked when the
  // DecoderBuffer is no longer in use and the attached secure buffer can be
  // released.
  virtual void ReleaseSecureBuffer(uint64_t secure_handle);

  // Set the DMA coherency of the video decoder buffers. Only relevant for
  // V4L2.
  virtual void SetDmaIncoherentV4L2(bool incoherent) {}

  // The VideoDecoderPipeline can use this to query a decoder for an upper bound
  // on the size of the frame pool that the decoder writes into. The default
  // implementation indicates no limit.
  virtual size_t GetMaxOutputFramePoolSize() const;

  // Implementers should override the other Initialize(). The decoder
  // pipeline wants a FrameResource, not a VideoFrame.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) final;

  // VideoDecoderMixins should implement the following Initialize instead of
  // VideoFrame::Initialize. This lets the decoder use FrameResource instead
  // of VideoFrame as an output type.
  virtual void Initialize(const VideoDecoderConfig& config,
                          bool low_delay,
                          CdmContext* cdm_context,
                          InitCB init_cb,
                          const PipelineOutputCB& output_cb,
                          const WaitingCB& waiting_cb) = 0;

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
  // |renderable_fourccs| is the list of formats that VideoDecoderPipeline may
  // use when outputting frames, in order of preference.
  static std::unique_ptr<VideoDecoder> Create(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<FrameResourceConverter> frame_converter,
      std::vector<Fourcc> renderable_fourccs,
      std::unique_ptr<MediaLog> media_log,
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      bool in_video_decoder_process);
  // Same idea but creates a VideoDecoderPipeline instance intended to be
  // adapted or bridged to a VideoDecodeAccelerator interface, for ARC clients.
  static std::unique_ptr<VideoDecoder> CreateForVDAAdapterForARC(
      const gpu::GpuDriverBugWorkarounds& workarounds,
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::vector<Fourcc> renderable_fourccs);

  static std::unique_ptr<VideoDecoder> CreateForTesting(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<MediaLog> media_log,
      bool ignore_resolution_changes_to_smaller_for_testing = false);

  static std::vector<Fourcc> DefaultPreferredRenderableFourccs();

  // Ensures that the video decoder supported configurations are known. When
  // they are, |cb| is called with a PendingRemote that corresponds to the same
  // connection as |oop_video_decoder| (which may be |oop_video_decoder|
  // itself). If |oop_video_decoder| is valid, the supported configurations are
  // those of an out-of-process video decoder (and in this case,
  // |oop_video_decoder| may be used internally to query those configurations).
  // Otherwise, the supported configurations are those of an in-process,
  // platform-specific decoder (e.g., VaapiVideoDecoder or V4L2VideoDecoder).
  //
  // |cb| is called with |oop_video_decoder| before NotifySupportKnown() returns
  // if the supported configurations are already known.
  //
  // This method is thread- and sequence-safe. |cb| is always called on the same
  // sequence as NotifySupportKnown().
  static void NotifySupportKnown(
      mojo::PendingRemote<stable::mojom::StableVideoDecoder> oop_video_decoder,
      base::OnceCallback<
          void(mojo::PendingRemote<stable::mojom::StableVideoDecoder>)> cb);

  static std::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs(
      VideoDecoderType decoder_type,
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
  void NotifyEstimatedMaxDecodeRequests(int num) override;
  CroStatus::Or<ImageProcessor::PixelLayoutCandidate> PickDecoderOutputFormat(
      const std::vector<ImageProcessor::PixelLayoutCandidate>& candidates,
      const gfx::Rect& decoder_visible_rect,
      const gfx::Size& decoder_natural_size,
      std::optional<gfx::Size> output_size,
      size_t num_codec_reference_frames,
      bool use_protected,
      bool need_aux_frame_pool,
      std::optional<DmabufVideoFramePool::CreateFrameCB> allocator) override;

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
      std::unique_ptr<FrameResourceConverter> frame_converter,
      std::vector<Fourcc> renderable_fourccs,
      std::unique_ptr<MediaLog> media_log,
      CreateDecoderFunctionCB create_decoder_function_cb,
      bool uses_oop_video_decoder,
      bool in_video_decoder_process);

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
  void OnFrameDecoded(scoped_refptr<FrameResource> frame);
  // Called when |image_processor_| finishes processing a frame.
  void OnFrameProcessed(scoped_refptr<FrameResource> frame);
  // Called when |frame_converter_| finishes converting a frame.
  void OnFrameConverted(scoped_refptr<VideoFrame> video_frame);
  // Called when |decoder_| invokes the waiting callback.
  void OnDecoderWaiting(WaitingReason reason);

  // Return true if the pipeline has pending frames that are returned from
  // |decoder_| but haven't been passed to the client.
  // i.e. |image_processor_| or |frame_converter_| has pending frames.
  bool HasPendingFrames() const;

  // Call VideoDecoderMixin::ApplyResolutionChange() when we need to.
  void CallApplyResolutionChangeIfNeeded();

  // Calls the client flush callback if there is a flush in progress and there
  // are no pending frames in the pipeline. If |override_status| is not nullopt,
  // we use it as the DecoderStatus to call the client flush callback with.
  // Otherwise, we use the original DecoderStatus passed by the underlying
  // decoder at the moment it notified us that the flush was completed at that
  // level. This is useful to handle cases like the following: suppose the
  // underlying decoder tells us a flush was completed without problems but we
  // can't call the client flush callback yet because there are pending frames
  // in the pipeline (perhaps in the image processor); then later, we get a
  // reset request; once the reset request is completed, we can call the client
  // flush callback but we should pass a DecoderStatus of kAborted instead of
  // kOk. In this scenario, the original DecoderStatus is kOk and
  // *|override_status| is kAborted.
  void CallFlushCbIfNeeded(std::optional<DecoderStatus> override_status);

#if BUILDFLAG(IS_CHROMEOS)
  // Callback for when transcryption of a buffer completes.
  void OnBufferTranscrypted(scoped_refptr<DecoderBuffer> transcrypted_buffer,
                            DecodeCB decode_callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // Used to determine the decoder's maximum output frame pool size.
  size_t GetDecoderMaxOutputFramePoolSize() const;

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
  std::unique_ptr<FrameResourceConverter> frame_converter_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // The set of output formats allowed to be used in order of preference.
  // VideoDecoderPipeline may perform copies to convert from the decoder's
  // output to one of these formats.
  const std::vector<Fourcc> renderable_fourccs_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  const std::unique_ptr<MediaLog> media_log_;

#if BUILDFLAG(IS_CHROMEOS)
  // The transcryptor for transcrypting DecoderBuffers when needed by the HW
  // decoder implementation.
  std::unique_ptr<DecoderBufferTranscryptor> buffer_transcryptor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // While |drop_transcrypted_buffers_| is true, we don't pass transcrypted
  // buffers to the |decoder_|, and instead, we call the corresponding decode
  // callback with kAborted. This is the case while a reset is ongoing, and it's
  // needed because the VideoDecoderMixin interface (by virtue of deriving from
  // VideoDecoder) requires that no VideoDecoder calls are made until the reset
  // closure has been executed.
  bool drop_transcrypted_buffers_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_) = false;
#endif  // BUILDFLAG(IS_CHROMEOS)

  // The current video decoder implementation. Valid after initialization is
  // successfully done.
  std::unique_ptr<VideoDecoderMixin> decoder_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  static constexpr int kDefaultMaxDecodeRequests = 8;
  std::atomic_int decoder_max_decode_requests_ = kDefaultMaxDecodeRequests;

  // Only used after initialization on |decoder_task_runner_|.
  CreateDecoderFunctionCB create_decoder_function_cb_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // Callbacks provided by the client. Used on |decoder_task_runner_|.
  // The callback methods themselves are exercised on |client_task_runner_|.
  OutputCB client_output_cb_ GUARDED_BY_CONTEXT(decoder_sequence_checker_);
  // For the flush callback, we keep a couple of things: the callback itself
  // provided by the client and the DecoderStatus obtained from the underlying
  // decoder at the moment it notifies the VideoDecoderPipeline that a flush has
  // been completed.
  struct ClientFlushCBState {
    ClientFlushCBState(DecodeCB flush_cb, DecoderStatus decoder_decode_status);
    ~ClientFlushCBState();
    DecodeCB flush_cb;
    const DecoderStatus decoder_decode_status;
  };
  std::optional<ClientFlushCBState> client_flush_cb_state_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
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

  // |oop_decoder_can_read_without_stalling_| is accessed from multiple
  // sequences: it's set on the decoder sequence for every frame we get from the
  // |decoder_|, and it's read on the client sequence.
  std::atomic<bool> oop_decoder_can_read_without_stalling_;

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

  // Set to true when the underlying |decoder_| is an OOPVideoDecoder.
  const bool uses_oop_video_decoder_;

  // Set to true to bypass checks for encrypted content support for testing.
  bool allow_encrypted_content_for_testing_ = false;

  // See VP9Decoder for information on this.
  bool ignore_resolution_changes_to_smaller_for_testing_ = false;

  // If we will need to tell the DecoderBufferTranscryptor to do VP9 superframe
  // splitting.
  bool decryption_needs_vp9_superframe_splitting_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_) = false;

  base::WeakPtr<VideoDecoderPipeline> decoder_weak_this_;
  // The weak pointer of this, bound to |decoder_task_runner_|.
  base::WeakPtrFactory<VideoDecoderPipeline> decoder_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
