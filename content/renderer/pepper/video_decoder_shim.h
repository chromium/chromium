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
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_decoder_config.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "media/video/video_decode_accelerator.h"
#include "ppapi/c/pp_codecs.h"

namespace viz {
class ContextProviderCommandBuffer;
}

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
  void AssignPictureBuffers(const std::vector<media::PictureBuffer>& buffers);
  void ReusePictureBuffer(int32_t picture_buffer_id);
  void Flush();
  void Reset();
  void Destroy();

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
                       shared_main_thread_context_provider,
                   scoped_refptr<viz::ContextProviderCommandBuffer>
                       pepper_video_decode_context_provider);

  void OnInitializeFailed();
  void OnDecodeComplete(int32_t result, absl::optional<uint32_t> decode_id);
  void OnOutputComplete(std::unique_ptr<PendingFrame> frame);
  void SendPictures();
  void OnResetComplete();
  void NotifyCompletedDecodes();
  void DismissTexture(uint32_t texture_id);
  void DeleteTexture(uint32_t texture_id);
  // Call this whenever we change GL state that the plugin relies on, such as
  // creating picture textures.
  void FlushCommandBuffer();

  std::unique_ptr<DecoderImpl> decoder_impl_;
  State state_;

  PepperVideoDecoderHost* host_;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;
  scoped_refptr<viz::ContextProviderCommandBuffer>
      shared_main_thread_context_provider_;
  scoped_refptr<viz::ContextProviderCommandBuffer>
      pepper_video_decode_context_provider_;

  // The current decoded frame size.
  gfx::Size texture_size_;
  // Map that takes the plugin's GL texture id to the renderer's mailbox.
  using IdToMailboxMap = std::unordered_map<uint32_t, gpu::Mailbox>;
  IdToMailboxMap texture_mailbox_map_;
  // Available textures (these are plugin ids.)
  using TextureIdSet = std::unordered_set<uint32_t>;
  TextureIdSet available_textures_;
  // Track textures that are no longer needed (these are plugin ids.)
  TextureIdSet textures_to_dismiss_;

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
