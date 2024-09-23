// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_H_
#define MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_H_

#include <list>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_preferences.h"
#include "media/base/callback_registry.h"
#include "media/base/status.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/base/video_decoder.h"
#include "media/base/video_types.h"
#include "media/gpu/command_buffer_helper.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/windows/d3d11_decoder_configurator.h"
#include "media/gpu/windows/d3d11_h264_accelerator.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_texture_selector.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"
#include "media/gpu/windows/d3d11_video_decoder_wrapper.h"
#include "media/gpu/windows/d3d11_video_frame_mailbox_release_helper.h"
#include "media/gpu/windows/d3d11_vp9_accelerator.h"
#include "media/gpu/windows/d3d_com_defs.h"

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

class D3D11PictureBuffer;
class D3D11VideoDecoderTest;
class MediaLog;

// Video decoder that uses D3D11 directly.  It is intended that this class will
// run the decoder on whatever thread it lives on.  However, at the moment, it
// only works if it's on the gpu main thread.
class MEDIA_GPU_EXPORT D3D11VideoDecoder : public VideoDecoder,
                                           public D3D11VideoDecoderClient {
 public:
  enum class D3DVersion { kD3D11, kD3D12 };

  // Callback to get a D3D11/12 device.
  using GetD3DDeviceCB = base::RepeatingCallback<ComUnknown(D3DVersion)>;

  // List of configs that we'll check against when initializing.  This is only
  // needed since GpuMojoMediaClient merges our supported configs with the VDA
  // supported configs.
  using SupportedConfigs = std::vector<SupportedVideoDecoderConfig>;

  // |helper| must be called from |gpu_task_runner|.
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
      GetD3DDeviceCB get_d3d_device_cb,
      SupportedConfigs supported_configs);

  D3D11VideoDecoder(const D3D11VideoDecoder&) = delete;
  D3D11VideoDecoder& operator=(const D3D11VideoDecoder&) = delete;

  // VideoDecoder implementation:
  VideoDecoderType GetDecoderType() const override;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure closure) override;
  bool NeedsBitstreamConversion() const override;
  bool CanReadWithoutStalling() const override;
  int GetMaxDecodeRequests() const override;

  // D3D11VideoDecoderClient implementation.
  D3D11PictureBuffer* GetPicture() override;
  void UpdateTimestamp(D3D11PictureBuffer* picture_buffer) override;
  bool OutputResult(const CodecPicture* picture,
                    D3D11PictureBuffer* picture_buffer) override;
  D3DVideoDecoderWrapper* GetWrapper() override;

  bool ResetD3DVideoDecoder();

  static bool GetD3D11FeatureLevel(
      ComD3D11Device dev,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      D3D_FEATURE_LEVEL* feature_level);

  // Return the set of video decoder configs that we support.
  static std::vector<SupportedVideoDecoderConfig>
  GetSupportedVideoDecoderConfigs(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      GetD3DDeviceCB get_d3d_device_cb);

 protected:
  // Owners should call Destroy(). This is automatic via
  // std::default_delete<media::VideoDecoder> when held by a
  // std::unique_ptr<media::VideoDecoder>.
  ~D3D11VideoDecoder() override;

 private:
  friend class D3D11VideoDecoderTest;

  D3D11VideoDecoder(
      scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
      std::unique_ptr<MediaLog> media_log,
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
      base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()>
          get_helper_cb,
      GetD3DDeviceCB get_d3d_device_cb,
      SupportedConfigs supported_configs);

  // Receive |buffer|, that is now unused by the client.
  void ReceivePictureBufferFromClient(scoped_refptr<D3D11PictureBuffer> buffer);

  // Picture buffer and related gpu resource initialization done.
  void PictureBufferGPUResourceInitDone(
      scoped_refptr<D3D11PictureBuffer> buffer);

  // Called when the gpu side of initialization is complete.
  void OnGpuInitComplete(
      bool success,
      D3D11VideoFrameMailboxReleaseHelper::ReleaseMailboxCB release_mailbox_cb);

  // Run the decoder loop.
  void DoDecode();

  // Instantiate |accelerated_video_decoder_| based on the video profile.
  // Returns false if the codec is unsupported.
  bool InitializeAcceleratedDecoder(const VideoDecoderConfig& config);

  // Query the video device for a specific decoder ID.
  bool DeviceHasDecoderID(GUID decoder_guid);

  // Create new PictureBuffers.  Currently, this completes synchronously, but
  // really should have an async interface since it must do some work on the
  // gpu main thread.
  void CreatePictureBuffers();

  // Measure the number of picture buffers that are currently unused, and see if
  // it's smaller than the minimum we've seen since we've allocated them.
  void MeasurePictureBufferUsage();

  // Log the current measurement of picture buffer usage, as measured by
  // `MeasurePictureBufferUsage()`, and clear the measurement.  Do nothing,
  // successfully, if no measurement has been made.
  void LogPictureBufferUsage();

  // Log the LUID of the adapter used for decoding.
  void LogDecoderAdapterLUID();

  // Create the D3DVideoDecoderWrapper according to the version level.
  std::unique_ptr<D3DVideoDecoderWrapper> CreateD3DVideoDecoderWrapper(
      D3D11DecoderConfigurator* decoder_configurator,
      uint8_t bit_depth);

  std::unique_ptr<MediaLog> media_log_;

  enum class State {
    // Initializing resources required to create a codec.
    kInitializing,

    // Initialization has completed and we're running. This is the only state
    // in which |codec_| might be non-null. If |codec_| is null, a codec
    // creation is pending.
    kRunning,

    // A fatal error occurred. A terminal state.
    kError,
  };

  // Enter the kError state. This will fail any pending |init_cb_| and/or
  // pending decodes as well. |opt_decoder_code| can be optionally provided for
  // a more descriptive reason passed back up to the decoder stream rather than
  // just kFailed.
  void NotifyError(
      D3D11Status reason,
      DecoderStatus::Codes opt_decoder_code = DecoderStatus::Codes::kFailed);

  // Posts |status| to any pending initialization or decode callbacks.
  void PostDecoderStatus(DecoderStatus status);

  // Mailbox release helper; which lives on the GPU main thread. Note: This must
  // be ref counted to outlive D3D11VideoDecoder since each output VideoFrame
  // uses it to wait on a SyncToken during mailbox release.
  scoped_refptr<D3D11VideoFrameMailboxReleaseHelper> mailbox_release_helper_;

  // GPU main thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  // Task runner on which |this| lives.
  scoped_refptr<base::SequencedTaskRunner> decoder_task_runner_;

  gpu::GpuPreferences gpu_preferences_;
  gpu::GpuDriverBugWorkarounds gpu_workarounds_;

  // During init, these will be set.
  VideoDecoderConfig config_;
  InitCB init_cb_;
  OutputCB output_cb_;

  // Callback to be used as a release CB for VideoFrames.  Be sure to
  // base::BindPostTaskToCurrentDefault the closure that it takes.
  D3D11VideoFrameMailboxReleaseHelper::ReleaseMailboxCB release_mailbox_cb_;

  // Right now, this is used both for the video decoder and for display.  In
  // the future, this should only be for the video decoder.  We should use
  // the ANGLE device for display (plus texture sharing, if needed).
  GetD3DDeviceCB get_d3d_device_cb_;

  // These may be accessed from |decoder_task_runner_|, since the angle device
  // is in multi-threaded mode.  Just be sure not to set any global state.
  ComD3D11Device device_;
  ComD3D11DeviceContext device_context_;
  ComD3D11VideoDevice video_device_;

  // D3D11 version on this device.
  D3D_FEATURE_LEVEL usable_feature_level_;

  std::unique_ptr<AcceleratedVideoDecoder> accelerated_video_decoder_;

  std::unique_ptr<D3D11DecoderConfigurator> decoder_configurator_;

  std::unique_ptr<TextureSelector> texture_selector_;

  std::list<std::pair<scoped_refptr<DecoderBuffer>, DecodeCB>>
      input_buffer_queue_;
  scoped_refptr<DecoderBuffer> current_buffer_;
  DecodeCB current_decode_cb_;
  base::TimeDelta current_timestamp_;

  // Must be called on the gpu main thread.  So, don't call it from here,
  // since we don't know what thread we're on.
  base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb_;

  // It would be nice to unique_ptr these, but we give a ref to the VideoFrame
  // so that the texture is retained until the mailbox is opened.
  std::vector<scoped_refptr<D3D11PictureBuffer>> picture_buffers_;

  State state_ = State::kInitializing;

  // Profile of the video being decoded.
  VideoCodecProfile profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;

  // Callback to get a command buffer helper.  Must be called from the gpu
  // main thread only.
  base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb_;

  // Entire class should be single-sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  SupportedConfigs supported_configs_;

  // Should we use shared handles for WebGPU interop or if using Graphite.
  bool use_shared_handle_ = false;

  // Should we use multiple single textures for the decoder output (true) or one
  // texture with multiple array slices (false)?
  bool use_single_video_decoder_texture_ = false;

  std::unique_ptr<D3DVideoDecoderWrapper> d3d_video_decoder_wrapper_;

  // The currently configured bit depth for the decoder. When this changes we
  // need to recreate the decoder.
  uint8_t bit_depth_ = 8u;

  // The currently configured color space for the decoder. When this changes we
  // need to recreate the decoder.
  VideoColorSpace color_space_;

  // The currently configured chroma sampling format on the accelerator. When
  // this changes we need to recreate the decoder.
  VideoChromaSampling chroma_sampling_ = VideoChromaSampling::k420;

  // If set, this is the minimum number of picture buffers that we've seen
  // since the last time it was logged to UMA that are unused by both the
  // client and the decoder.  If unset, then no measurement has been made.
  std::optional<int> min_unused_buffers_;

  // Picture buffer usage is measured periodically after some number of decodes.
  // This tracks how many until the next measurement.  It's used strictly to
  // rate limit the measurements, so we don't spent too much time counting
  // unused picture buffers.
  int decode_count_until_picture_buffer_measurement_ = 0;

  base::WeakPtrFactory<D3D11VideoDecoder> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D11_VIDEO_DECODER_H_
