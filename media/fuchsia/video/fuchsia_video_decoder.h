// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_DECODER_H_
#define MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_DECODER_H_

#include <deque>
#include <memory>
#include <vector>

#include <fuchsia/media/cpp/fidl.h>
#include <lib/zx/eventpair.h>

#include "base/memory/scoped_refptr.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "media/fuchsia/common/sysmem_buffer_stream.h"
#include "media/fuchsia/common/sysmem_client.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace gfx {
class ClientNativePixmapFactory;
}  // namespace gfx

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

namespace mojom {
class FuchsiaMediaCodecProvider;
}  // namespace mojom

class MEDIA_EXPORT FuchsiaVideoDecoder : public VideoDecoder,
                                         public SysmemBufferStream::Sink,
                                         public StreamProcessorHelper::Client {
 public:
  FuchsiaVideoDecoder(
      scoped_refptr<viz::RasterContextProvider> raster_context_provider,
      const mojo::SharedRemote<media::mojom::FuchsiaMediaCodecProvider>&
          media_codec_provider,
      bool allow_overlays);
  ~FuchsiaVideoDecoder() override;

  FuchsiaVideoDecoder(const FuchsiaVideoDecoder&) = delete;
  FuchsiaVideoDecoder& operator=(const FuchsiaVideoDecoder&) = delete;

  // Decoder implementation.
  bool IsPlatformDecoder() const override;
  bool SupportsDecryption() const override;
  VideoDecoderType GetDecoderType() const override;

  // VideoDecoder implementation.
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

  void SetClientNativePixmapFactoryForTests(
      std::unique_ptr<gfx::ClientNativePixmapFactory> factory);

 private:
  class OutputMailbox;

  DecoderStatus InitializeSysmemBufferStream(bool is_encrypted,
                                             CdmContext* cdm_context,
                                             bool* secure_mode);

  // SysmemBufferStream::Sink implementation.
  void OnSysmemBufferStreamBufferCollectionToken(
      fuchsia::sysmem2::BufferCollectionTokenPtr token) override;
  void OnSysmemBufferStreamOutputPacket(
      StreamProcessorHelper::IoPacket packet) override;
  void OnSysmemBufferStreamEndOfStream() override;
  void OnSysmemBufferStreamError() override;
  void OnSysmemBufferStreamNoKey() override;

  // StreamProcessorHelper::Client implementation.
  void OnStreamProcessorAllocateOutputBuffers(
      const fuchsia::media::StreamBufferConstraints& stream_constraints)
      override;
  void OnStreamProcessorEndOfStream() override;
  void OnStreamProcessorOutputFormat(
      fuchsia::media::StreamOutputFormat format) override;
  void OnStreamProcessorOutputPacket(
      StreamProcessorHelper::IoPacket packet) override;
  void OnStreamProcessorNoKey() override;
  void OnStreamProcessorError() override;

  // Calls next callback in the |decode_callbacks_| queue.
  void CallNextDecodeCallback();

  // Drops all pending input buffers and then calls all pending DecodeCB with
  // |status|. Returns true if the decoder still exists.
  bool DropInputQueue(DecoderStatus status);

  // Called on errors to shutdown the decoder and notify the client.
  void OnError();

  // Callback for SysmemBufferCollection::CreateSharedToken(), used to send the
  // sysmem buffer collection token to the GPU process.
  void SetBufferCollectionTokenForGpu(
      fuchsia::sysmem2::BufferCollectionTokenPtr token);

  // Called by OutputMailbox to signal that the output buffer can be reused.
  void ReleaseOutputPacket(StreamProcessorHelper::IoPacket packet);

  // Releases BufferCollection used for output buffers if any.
  void ReleaseOutputBuffers();

  const scoped_refptr<viz::RasterContextProvider> raster_context_provider_;
  const mojo::SharedRemote<media::mojom::FuchsiaMediaCodecProvider>
      media_codec_provider_;

  const bool use_overlays_for_video_;

  OutputCB output_cb_;
  WaitingCB waiting_cb_;

  std::unique_ptr<SysmemBufferStream> sysmem_buffer_stream_;

  // Initialized in InitializeSysmemBufferStream()
  size_t max_decoder_requests_ = 1;

  VideoDecoderConfig current_config_;

  std::unique_ptr<StreamProcessorHelper> decoder_;

  SysmemAllocatorClient sysmem_allocator_;
  std::unique_ptr<gfx::ClientNativePixmapFactory> client_native_pixmap_factory_;

  // Callbacks for pending Decode() request.
  std::deque<DecodeCB> decode_callbacks_;

  // Input buffers for |decoder_|.
  std::unique_ptr<SysmemCollectionClient> input_buffer_collection_;

  // Output buffers for |decoder_|.
  fuchsia::media::VideoUncompressedFormat output_format_;
  std::unique_ptr<SysmemCollectionClient> output_buffer_collection_;
  zx::eventpair output_buffer_collection_handle_;
  std::vector<OutputMailbox*> output_mailboxes_;

  // Set to true when the output buffers are protected.
  bool protected_output_ = false;

  size_t num_used_output_buffers_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // WeakPtrFactory used to schedule CallNextDecodeCallbacks(). These pointers
  // are discarded in DropInputQueue() in order to avoid calling
  // Decode() callback when the decoder queue is discarded.
  base::WeakPtrFactory<FuchsiaVideoDecoder> decode_callbacks_weak_factory_{
      this};
};

}  // namespace media

#endif  // MEDIA_FUCHSIA_VIDEO_FUCHSIA_VIDEO_DECODER_H_
