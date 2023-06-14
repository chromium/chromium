// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_VIDEO_DECODER_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_VIDEO_DECODER_H_

#include <CoreMedia/CoreMedia.h>
#include <VideoToolbox/VideoToolbox.h>

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/codec_picture.h"
#include "media/gpu/mac/video_toolbox_frame_converter.h"
#include "media/gpu/media_gpu_export.h"

namespace gpu {
class CommandBufferStub;
}  // namespace gpu

namespace media {

class AcceleratedVideoDecoder;
class MediaLog;
class VideoToolboxDecompressionInterface;

class MEDIA_GPU_EXPORT VideoToolboxVideoDecoder : public VideoDecoder {
 public:
  using GetCommandBufferStubCB =
      base::RepeatingCallback<gpu::CommandBufferStub*()>;

  VideoToolboxVideoDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log,
      scoped_refptr<base::SequencedTaskRunner> gpu_task_runner,
      GetCommandBufferStubCB get_stub_cb);

  ~VideoToolboxVideoDecoder() override;

  // VideoDecoder implementation.
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;
  int GetMaxDecodeRequests() const override;
  VideoDecoderType GetDecoderType() const override;

 private:
  // Shut down and enter a permanent error state.
  void NotifyError(DecoderStatus status);

  // Drop all state, calling decode callbacks with |status|.
  void ResetInternal(DecoderStatus status);

  // Match |output_queue_| entries to |output_frames_| and output them.
  void ProcessOutputs();

  // Call |decode_cb_| entries until the correct backpressure is achieved.
  void ReleaseDecodeCallbacks();

  // |accelerator_| callbacks.
  void OnAcceleratorDecode(base::ScopedCFTypeRef<CMSampleBufferRef> sample,
                           scoped_refptr<CodecPicture> picture);
  void OnAcceleratorOutput(scoped_refptr<CodecPicture> picture);

  // |video_toolbox_| callbacks.
  void OnVideoToolboxOutput(base::ScopedCFTypeRef<CVImageBufferRef> image,
                            void* context);
  void OnVideoToolboxError(DecoderStatus status);

  // |converter_| callbacks.
  void OnConverterOutput(scoped_refptr<VideoFrame> frame, void* context);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<MediaLog> media_log_;
  scoped_refptr<base::SequencedTaskRunner> gpu_task_runner_;
  GetCommandBufferStubCB get_stub_cb_;

  bool has_error_ = false;

  VideoDecoderConfig config_;
  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB flush_cb_;

  // Pending decode callbacks. These are released in decode order, keeping
  // the total number the same as the number of pending decodes in
  // |video_toolbox_|. There is no mapping to actual decode requests, it is
  // only a backpressure mechanism.
  base::queue<DecodeCB> decode_cbs_;

  // Used to link re-entrant OnAcceleratorDecode() callbacks to Decode() calls.
  scoped_refptr<DecoderBuffer> active_decode_;

  std::unique_ptr<AcceleratedVideoDecoder> accelerator_;
  std::unique_ptr<VideoToolboxDecompressionInterface> video_toolbox_;
  scoped_refptr<VideoToolboxFrameConverter> converter_;

  // Metadata for decodes that are currently in |video_toolbox_|.
  struct DecodeMetadata {
    base::TimeDelta timestamp;
  };
  base::flat_map<void*, DecodeMetadata> decode_metadata_;

  // The output order of decodes.
  // Note: outputs are created after decodes.
  base::queue<scoped_refptr<CodecPicture>> output_queue_;

  // Frames that have completed conversion.
  base::flat_map<void*, scoped_refptr<VideoFrame>> output_frames_;

  // Convertersion callbacks are invalidated during resets.
  base::WeakPtrFactory<VideoToolboxVideoDecoder> converter_weak_this_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_VIDEO_DECODER_H_
