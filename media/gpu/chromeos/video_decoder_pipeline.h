// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "media/base/cdm_context.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_with_pool.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "media/gpu/chromeos/decoder_buffer_transcryptor.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
namespace base {
class SequencedTaskRunner;
}

namespace gpu {
class GpuDriverBugWorkarounds;
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

    // Get the video frame pool without passing the ownership. Return nullptr if
    // the decoder is responsible for allocating its own frames.
    virtual DmabufVideoFramePool* GetVideoFramePool() const = 0;

    // After this method is called from |decoder_|, the client needs to call
    // VideoDecoderMixin::ApplyResolutionChange() when all pending frames are
    // flushed.
    virtual void PrepareChangeResolution() = 0;

    // Return a valid format and size for |decoder_| output from given
    // |candidates| and the visible rect. The size might be modified from the
    // ones provided originally to accommodate the needs of the pipeline.
    // Return absl::nullopt if no valid format is found.
    virtual absl::optional<std::pair<Fourcc, gfx::Size>>
    PickDecoderOutputFormat(
        const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
        const gfx::Rect& visible_rect) = 0;
  };

  VideoDecoderMixin(
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
      base::WeakPtr<VideoDecoderMixin::Client> client);
  ~VideoDecoderMixin() override;

  // After DecoderInterface calls |prepare_change_resolution_cb| passed
  // from the constructor, this method is called when the pipeline flushes
  // pending frames.
  virtual void ApplyResolutionChange() = 0;

  // For protected content implementations that require transcryption of the
  // content before being sent into the HW decoders. (Currently only used by
  // AMD). Default implementation returns false.
  virtual bool NeedsTranscryption();

 protected:
  const std::unique_ptr<MediaLog> media_log_;

  // Decoder task runner. All public methods of
  // VideoDecoderMixin are executed at this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // The WeakPtr client instance, bound to |decoder_task_runner_|.
  base::WeakPtr<VideoDecoderMixin::Client> client_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoderMixin);
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
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log);

  static absl::optional<SupportedVideoDecoderConfigs> GetSupportedConfigs(
      const gpu::GpuDriverBugWorkarounds& workarounds);

  ~VideoDecoderPipeline() override;
  static void DestroyAsync(std::unique_ptr<VideoDecoderPipeline>);

  // VideoDecoder implementation
  VideoDecoderType GetDecoderType() const override;
  bool IsPlatformDecoder() const override;
  int GetMaxDecodeRequests() const override;
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
  // After picking a format, it instantiates an |image_processor_| if none of
  // format in |candidates| is renderable and an ImageProcessor can convert a
  // candidate to renderable format.
  absl::optional<std::pair<Fourcc, gfx::Size>> PickDecoderOutputFormat(
      const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
      const gfx::Rect& visible_rect) override;

 private:
  friend class VideoDecoderPipelineTest;

  VideoDecoderPipeline(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log,
      CreateDecoderFunctionCB create_decoder_function_cb);

  void InitializeTask(const VideoDecoderConfig& config,
                      CdmContext* cdm_context,
                      InitCB init_cb,
                      const OutputCB& output_cb,
                      const WaitingCB& waiting_cb);
  void ResetTask(base::OnceClosure reset_cb);
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb);

  void OnInitializeDone(InitCB init_cb, CdmContext* cdm_context, Status status);

  void OnDecodeDone(bool eos_buffer, DecodeCB decode_cb, Status status);
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
  void CallFlushCbIfNeeded(DecodeStatus status);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Callback for when transcryption of a buffer completes.
  void OnBufferTranscrypted(scoped_refptr<DecoderBuffer> transcrypted_buffer,
                            DecodeCB decode_callback);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  // The image processor is only created when the decoder cannot output frames
  // with renderable format.
  std::unique_ptr<ImageProcessorWithPool> image_processor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  // The frame converter passed from the client, otherwise used and destroyed on
  // |decoder_task_runner_|.
  std::unique_ptr<VideoFrameConverter> frame_converter_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);

  const std::unique_ptr<MediaLog> media_log_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The transcryptor for transcrypting DecoderBuffers when needed by the HW
  // decoder implementation.
  std::unique_ptr<DecoderBufferTranscryptor> buffer_transcryptor_
      GUARDED_BY_CONTEXT(decoder_sequence_checker_);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

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

  // True if we need to notify |decoder_| that the pipeline is flushed via
  // VideoDecoderMixin::ApplyResolutionChange().
  bool need_apply_new_resolution GUARDED_BY_CONTEXT(decoder_sequence_checker_) =
      false;

  // True if the decoder needs bitstream conversion before decoding.
  bool needs_bitstream_conversion_
      GUARDED_BY_CONTEXT(client_sequence_checker_) = false;

  // Set to true when any unexpected error occurs.
  bool has_error_ GUARDED_BY_CONTEXT(decoder_sequence_checker_) = false;

  // Set to true to bypass checks for encrypted content support for testing.
  bool allow_encrypted_content_for_testing_ = false;

  base::WeakPtr<VideoDecoderPipeline> decoder_weak_this_;
  // The weak pointer of this, bound to |decoder_task_runner_|.
  base::WeakPtrFactory<VideoDecoderPipeline> decoder_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
