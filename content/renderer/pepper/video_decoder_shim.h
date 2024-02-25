// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_
#define CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_decoder_config.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_codecs.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"

namespace content {

class PepperVideoDecoderHost;

// This class is a shim to wrap a media::VideoDecoder so that it can be used
// by PepperVideoDecoderHost. This class should be constructed, used, and
// destructed on the main (render) thread.
class VideoDecoderShim {
 public:
  static std::unique_ptr<VideoDecoderShim> Create(PepperVideoDecoderHost* host,
                                                  uint32_t texture_pool_size,
                                                  bool use_hw_decoder);

  VideoDecoderShim(const VideoDecoderShim&) = delete;
  VideoDecoderShim& operator=(const VideoDecoderShim&) = delete;

  ~VideoDecoderShim();

  bool Initialize(media::VideoCodecProfile profile);
  void Decode(media::BitstreamBuffer bitstream_buffer);
  void ReuseSharedImage(const gpu::Mailbox& mailbox, gfx::Size size);
  void Flush();
  void Reset();
  void Destroy();

  const scoped_refptr<viz::ContextProviderCommandBuffer>& context_provider()
      const {
    return shared_main_thread_context_provider_;
  }

 private:
  enum State {
    UNINITIALIZED,
    DECODING,
    FLUSHING,
    RESETTING,
  };

  struct PendingDecode;
  struct PendingFrame;
  class DecoderImpl;

  VideoDecoderShim(PepperVideoDecoderHost* host,
                   uint32_t texture_pool_size,
                   bool use_hw_decoder,
                   scoped_refptr<viz::ContextProviderCommandBuffer>
                       shared_main_thread_context_provider);

  void OnInitializeFailed();
  void OnDecodeComplete(int32_t result, std::optional<uint32_t> decode_id);
  void OnOutputComplete(std::unique_ptr<PendingFrame> frame);
  void SendSharedImages();
  void OnResetComplete();
  void NotifyCompletedDecodes();
  // Call this whenever we change GL state that the plugin relies on, such as
  // creating picture textures.
  void FlushCommandBuffer();

  std::unique_ptr<DecoderImpl> decoder_impl_;
  State state_;

  raw_ptr<PepperVideoDecoderHost> host_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<viz::ContextProviderCommandBuffer>
      shared_main_thread_context_provider_;

  // The current decoded frame size.
  gfx::Size texture_size_;

  std::vector<gpu::Mailbox> available_shared_images_;

  // Queue of completed decode ids, for notifying the host.
  using CompletedDecodeQueue = base::queue<uint32_t>;
  CompletedDecodeQueue completed_decodes_;

  // Queue of decoded frames that await rgb->yuv conversion.
  using PendingFrameQueue = base::queue<std::unique_ptr<PendingFrame>>;
  PendingFrameQueue pending_frames_;

  // The optimal number of textures to allocate for decoder_impl_.
  uint32_t texture_pool_size_;

  uint32_t num_pending_decodes_;

  const bool use_hw_decoder_;

  std::unique_ptr<media::PaintCanvasVideoRenderer> video_renderer_;

  base::WeakPtrFactory<VideoDecoderShim> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_VIDEO_DECODER_SHIM_H_
