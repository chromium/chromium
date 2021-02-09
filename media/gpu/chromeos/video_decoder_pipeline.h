// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor_with_pool.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SequencedTaskRunner;
}

namespace media {

class DmabufVideoFramePool;
class MediaLog;

// An interface that defines methods to operate on video decoder components
// inside the VideoDecoderPipeline. The interface is similar to
// media::VideoDecoder. The reason not using media::VideoDecoder is that some
// decoders might need to attach an image processor to perform frame
// processing, the output of VideoDecoder is not suitable when the
// intermediate output cannot be rendered by the compositor.
//
// Note: All methods and callbacks should be called on the same sequence.
class MEDIA_GPU_EXPORT DecoderInterface {
 public:
  using InitCB = base::OnceCallback<void(Status status)>;
  // TODO(crbug.com/998413): Replace VideoFrame to GpuMemoryBuffer-based
  // instance.
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>;
  using DecodeCB = VideoDecoder::DecodeCB;

  // Client interface of DecoderInterface.
  class MEDIA_GPU_EXPORT Client {
   public:
    Client() = default;
    virtual ~Client() = default;

    // Get the video frame pool without passing the ownership. Return nullptr if
    // the decoder is responsible for allocating its own frames.
    virtual DmabufVideoFramePool* GetVideoFramePool() const = 0;

    // After this method is called from |decoder_|, the client needs to call
    // DecoderInterface::ApplyResolutionChange() when all pending frames are
    // flushed.
    virtual void PrepareChangeResolution() = 0;

    // Return a valid format and size for |decoder_| output from given
    // |candidates| and the visible rect. The size might be modified from the
    // ones provided originally to accommodate the needs of the pipeline.
    // Return base::nullopt if no valid format is found.
    virtual base::Optional<std::pair<Fourcc, gfx::Size>>
    PickDecoderOutputFormat(
        const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
        const gfx::Rect& visible_rect) = 0;
  };

  DecoderInterface(scoped_refptr<base::SequencedTaskRunner> decoder_task_runner,
                   base::WeakPtr<DecoderInterface::Client> client);
  virtual ~DecoderInterface();

  // Initializes a DecoderInterface with the given |config|, executing the
  // |init_cb| upon completion. |output_cb| is called for each output frame
  // decoded by Decode().
  //
  // Note:
  // 1) DecoderInterface will be reinitialized if it was initialized before.
  // 2) This method should not be called during pending decode or reset.
  // 3) No DecoderInterface calls should be made before |init_cb| is executed
  //    successfully.
  // TODO(akahuang): Add an error notification method to handle misused case.
  // 4) |init_cb| may be called before this returns.
  virtual void Initialize(const VideoDecoderConfig& config,
                          CdmContext* cdm_context,
                          InitCB init_cb,
                          const OutputCB& output_cb,
                          const WaitingCB& waiting_cb) = 0;

  // Requests a |buffer| to be decoded. The decode result will be returned via
  // |decode_cb|.
  //
  // After decoding is finished the decoder calls |output_cb| specified in
  // Initialize() for each decoded frame. |output_cb| may be called before or
  // after |decode_cb|, including before Decode() returns.
  //
  // If |buffer| is an EOS buffer then the decoder must be flushed, i.e.
  // |output_cb| must be called for each frame pending in the queue and
  // |decode_cb| must be called after that. Callers will not call Decode()
  // again until after the flush completes.
  // TODO(akahuang): Add an error notification method to handle misused case.
  virtual void Decode(scoped_refptr<DecoderBuffer> buffer,
                      DecodeCB decode_cb) = 0;

  // Resets decoder state. All pending Decode() requests will be finished or
  // aborted before |closure| is called.
  // Note: No VideoDecoder calls should be made before |closure| is executed.
  // TODO(akahuang): Add an error notification method to handle misused case.
  virtual void Reset(base::OnceClosure closure) = 0;

  // After DecoderInterface calls |prepare_change_resolution_cb| passed
  // from the constructor, this method is called when the pipeline flushes
  // pending frames.
  virtual void ApplyResolutionChange() = 0;

 protected:
  // Decoder task runner. All public methods of
  // DecoderInterface are executed at this task runner.
  const scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  // The WeakPtr client instance, bound to |decoder_task_runner_|.
  base::WeakPtr<DecoderInterface::Client> client_;

  DISALLOW_COPY_AND_ASSIGN(DecoderInterface);
};

class MEDIA_GPU_EXPORT VideoDecoderPipeline : public VideoDecoder,
                                              public DecoderInterface::Client {
 public:
  using CreateDecoderFunction = std::unique_ptr<DecoderInterface> (*)(
      scoped_refptr<base::SequencedTaskRunner>,
      base::WeakPtr<DecoderInterface::Client>);
  using CreateDecoderFunctions = std::list<CreateDecoderFunction>;
  using GetCreateDecoderFunctionsCB =
      base::RepeatingCallback<CreateDecoderFunctions()>;

  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      std::unique_ptr<MediaLog> media_log,
      GetCreateDecoderFunctionsCB get_create_decoder_functions_cb);

  ~VideoDecoderPipeline() override;
  static void DestroyAsync(std::unique_ptr<VideoDecoderPipeline>);

  // VideoDecoder implementation
  VideoDecoderType GetDecoderType() const override;
  std::string GetDisplayName() const override;
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
  void Reset(base::OnceClosure closure) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;

  // DecoderInterface::Client implementation.
  DmabufVideoFramePool* GetVideoFramePool() const override;
  void PrepareChangeResolution() override;
  // After picking a format, it instantiates an |image_processor_| if none of
  // format in |candidates| is renderable and an ImageProcessor can convert a
  // candidate to renderable format.
  base::Optional<std::pair<Fourcc, gfx::Size>> PickDecoderOutputFormat(
      const std::vector<std::pair<Fourcc, gfx::Size>>& candidates,
      const gfx::Rect& visible_rect) override;

 private:
  friend class VideoDecoderPipelineTest;

  VideoDecoderPipeline(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      GetCreateDecoderFunctionsCB get_create_decoder_functions_cb);

  void InitializeTask(const VideoDecoderConfig& config,
                      CdmContext* cdm_context,
                      InitCB init_cb,
                      const OutputCB& output_cb,
                      const WaitingCB& waiting_cb);
  void ResetTask(base::OnceClosure closure);
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb);

  void CreateAndInitializeVD(VideoDecoderConfig config,
                             CdmContext* cdm_context,
                             const WaitingCB& waiting_cb,
                             Status parent_error);
  void OnInitializeDone(VideoDecoderConfig config,
                        CdmContext* cdm_context,
                        const WaitingCB& waiting_cb,
                        Status parent_error,
                        Status status);

  void OnDecodeDone(bool eos_buffer, DecodeCB decode_cb, Status status);
  void OnResetDone();
  void OnError(const std::string& msg);

  // Called when |decoder_| finishes decoding a frame.
  void OnFrameDecoded(scoped_refptr<VideoFrame> frame);
  // Called when |image_processor_| finishes processing a frame.
  void OnFrameProcessed(scoped_refptr<VideoFrame> frame);
  // Called when |frame_converter_| finishes converting a frame.
  void OnFrameConverted(scoped_refptr<VideoFrame> frame);

  // Return true if the pipeline has pending frames that are returned from
  // |decoder_| but haven't been passed to the client.
  // i.e. |image_processor_| or |frame_converter_| has pending frames.
  bool HasPendingFrames() const;

  // Call DecoderInterface::ApplyResolutionChange() when we need to.
  void CallApplyResolutionChangeIfNeeded();

  // Call |client_flush_cb_| with |status|.
  void CallFlushCbIfNeeded(DecodeStatus status);

  // Handle ImageProcessor error callback.
  void OnImageProcessorError();

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
  // the client should be created using this pool.
  // Used on |decoder_task_runner_|.
  std::unique_ptr<DmabufVideoFramePool> main_frame_pool_;

  // The image processor is only created when the decoder cannot output frames
  // with renderable format.
  std::unique_ptr<ImageProcessorWithPool> image_processor_;

  // The frame converter passed from the client. Destroyed on
  // |client_task_runner_|.
  std::unique_ptr<VideoFrameConverter> frame_converter_;

  // The current video decoder implementation. Valid after initialization is
  // successfully done.
  std::unique_ptr<DecoderInterface> decoder_;

  // |remaining_create_decoder_functions_| holds all the potential video decoder
  // creation functions. We try them all in the given order until one succeeds.
  // Only used after initialization on |decoder_sequence_checker_|.
  CreateDecoderFunctions remaining_create_decoder_functions_;

  // Callback from the client. These callback are called on
  // |client_task_runner_|.
  InitCB init_cb_;
  OutputCB client_output_cb_;
  DecodeCB client_flush_cb_;
  base::OnceClosure client_reset_cb_;

  // True if we need to notify |decoder_| that the pipeline is flushed via
  // DecoderInterface::ApplyResolutionChange().
  bool need_apply_new_resolution = false;

  // True if the decoder needs bitstream conversion before decoding.
  bool needs_bitstream_conversion_ = false;

  // Set to true when any unexpected error occurs.
  bool has_error_ = false;

  base::WeakPtr<VideoDecoderPipeline> client_weak_this_;
  base::WeakPtr<VideoDecoderPipeline> decoder_weak_this_;

  // The weak pointer of this, bound to |client_task_runner_|.
  base::WeakPtrFactory<VideoDecoderPipeline> client_weak_this_factory_{this};
  // The weak pointer of this, bound to |decoder_task_runner_|.
  base::WeakPtrFactory<VideoDecoderPipeline> decoder_weak_this_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
