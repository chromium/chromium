// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_
#define MEDIA_GPU_CHROMEOS_VIDEO_DECODER_PIPELINE_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/video_frame_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace gpu {
class GpuMemoryBufferFactory;
}  // namespace gpu

namespace media {

class DmabufVideoFramePool;

class MEDIA_GPU_EXPORT VideoDecoderPipeline : public VideoDecoder {
 public:
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
    using InitCB = base::OnceCallback<void(bool success)>;
    // TODO(crbug.com/998413): Replace VideoFrame to GpuMemoryBuffer-based
    // instance.
    using OutputCB = base::RepeatingCallback<void(scoped_refptr<VideoFrame>)>;
    using DecodeCB = base::OnceCallback<void(DecodeStatus)>;

    DecoderInterface() = default;
    virtual ~DecoderInterface() = default;

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
                            InitCB init_cb,
                            const OutputCB& output_cb) = 0;

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

    DISALLOW_COPY_AND_ASSIGN(DecoderInterface);
  };

  // Function signature for creating VideoDecoder.
  using CreateVDFunc = std::unique_ptr<DecoderInterface> (*)(
      scoped_refptr<base::SequencedTaskRunner>,
      base::RepeatingCallback<DmabufVideoFramePool*()>);
  using GetCreateVDFunctionsCB =
      base::RepeatingCallback<base::queue<CreateVDFunc>(CreateVDFunc)>;

  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory,
      GetCreateVDFunctionsCB get_create_vd_functions_cb);

  ~VideoDecoderPipeline() override;

  // VideoDecoder implementation
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

 private:
  // Get a list of the available functions for creating VideoDeocoder.
  static base::queue<CreateVDFunc> GetCreateVDFunctions(
      CreateVDFunc current_func);

  VideoDecoderPipeline(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner,
      std::unique_ptr<DmabufVideoFramePool> frame_pool,
      std::unique_ptr<VideoFrameConverter> frame_converter,
      gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory,
      GetCreateVDFunctionsCB get_create_vd_functions_cb);
  void Destroy() override;
  void DestroyTask();

  void InitializeTask(const VideoDecoderConfig& config,
                      InitCB init_cb,
                      const OutputCB& output_cb);
  void ResetTask(base::OnceClosure closure);
  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb);

  void CreateAndInitializeVD(base::queue<CreateVDFunc> create_vd_funcs,
                             VideoDecoderConfig config);
  void OnInitializeDone(base::queue<CreateVDFunc> create_vd_funcs,
                        VideoDecoderConfig config,
                        bool success);

  void OnDecodeDone(bool eos_buffer, DecodeCB decode_cb, DecodeStatus status);
  void OnResetDone();
  void OnFrameConverted(scoped_refptr<VideoFrame> frame);
  void OnError(const std::string& msg);

  void OnFrameDecoded(scoped_refptr<VideoFrame> frame);

  // Call |client_flush_cb_| with |status| if we need.
  void CallFlushCbIfNeeded(DecodeStatus status);

  // Get the video frame pool without passing the ownership.
  DmabufVideoFramePool* GetVideoFramePool() const;

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
  // Used to generate additional frame pools for intermediate results if
  // required. The instance is indirectly owned by GpuChildThread, therefore
  // alive as long as the GPU process is.
  gpu::GpuMemoryBufferFactory* const gpu_memory_buffer_factory_;

  // The frame converter passed from the client. Destroyed on
  // |client_task_runner_|.
  std::unique_ptr<VideoFrameConverter> frame_converter_;

  // The callback to get a list of function for creating DecoderInterface.
  GetCreateVDFunctionsCB get_create_vd_functions_cb_;

  // The current video decoder implementation. Valid after initialization is
  // successfully done.
  std::unique_ptr<DecoderInterface> decoder_;
  // The create function of |decoder_|. nullptr iff |decoder_| is nullptr.
  CreateVDFunc used_create_vd_func_ = nullptr;

  // Callback from the client. These callback are called on
  // |client_task_runner_|.
  InitCB init_cb_;
  OutputCB client_output_cb_;
  DecodeCB client_flush_cb_;
  base::OnceClosure client_reset_cb_;

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
