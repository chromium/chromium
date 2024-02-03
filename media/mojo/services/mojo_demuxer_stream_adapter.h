// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_ADAPTER_H_
#define MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_ADAPTER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/mojom/demuxer_stream.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace media {

class MojoDecoderBufferReader;

// This class acts as a MojoRendererService-side stub for a real DemuxerStream
// that is part of a Pipeline in a remote application. Roughly speaking, it
// takes a mojo::Remote<mojom::DemuxerStream> and exposes it as a DemuxerStream
// for use by media components.
class MEDIA_MOJO_EXPORT MojoDemuxerStreamAdapter : public DemuxerStream {
 public:
  // |demuxer_stream| is connected to the mojom::DemuxerStream that |this|
  // will
  //     become the client of.
  // |stream_ready_cb| will be invoked when |demuxer_stream| has fully
  //     initialized and |this| is ready for use.
  // NOTE: Illegal to call any methods until |stream_ready_cb| is invoked.
  MojoDemuxerStreamAdapter(
      mojo::PendingRemote<mojom::DemuxerStream> demuxer_stream,
      base::OnceClosure stream_ready_cb);

  MojoDemuxerStreamAdapter(const MojoDemuxerStreamAdapter&) = delete;
  MojoDemuxerStreamAdapter& operator=(const MojoDemuxerStreamAdapter&) = delete;

  ~MojoDemuxerStreamAdapter() override;

  // DemuxerStream implementation.
  void Read(uint32_t count, ReadCB read_cb) override;
  AudioDecoderConfig audio_decoder_config() override;
  VideoDecoderConfig video_decoder_config() override;
  Type type() const override;
  void EnableBitstreamConverter() override;
  bool SupportsConfigChanges() override;

 private:
  void OnStreamReady(Type type,
                     mojo::ScopedDataPipeConsumerHandle consumer_handle,
                     const std::optional<AudioDecoderConfig>& audio_config,
                     const std::optional<VideoDecoderConfig>& video_config);

  // The callback from |demuxer_stream_| that a read operation has completed.
  // |read_cb| is a callback from the client who invoked Read() on |this|.
  void OnBufferReady(Status status,
                     std::vector<mojom::DecoderBufferPtr> batch_buffers,
                     const std::optional<AudioDecoderConfig>& audio_config,
                     const std::optional<VideoDecoderConfig>& video_config);

  void OnBufferRead(scoped_refptr<DecoderBuffer> buffer);

  void UpdateConfig(const std::optional<AudioDecoderConfig>& audio_config,
                    const std::optional<VideoDecoderConfig>& video_config);

  // See constructor for descriptions.
  mojo::Remote<mojom::DemuxerStream> demuxer_stream_;
  base::OnceClosure stream_ready_cb_;

  // The last ReadCB received through a call to Read().
  // Used to store the results of OnBufferReady() in the event it is called
  // with Status::kConfigChanged and we don't have an up to date
  // AudioDecoderConfig yet. In that case we can't forward the results
  // on to the caller of Read() until OnAudioDecoderConfigChanged is observed.
  ReadCB read_cb_;

  // The current config.
  AudioDecoderConfig audio_config_;
  VideoDecoderConfig video_config_;

  Type type_ = Type::UNKNOWN;
  Status status_ = Status::kOk;

  size_t actual_read_count_ = 0;

  DemuxerStream::DecoderBufferVector buffer_queue_;

  std::vector<mojom::DecoderBufferPtr> batch_buffers_;

  std::unique_ptr<MojoDecoderBufferReader> mojo_decoder_buffer_reader_;

  base::WeakPtrFactory<MojoDemuxerStreamAdapter> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_ADAPTER_H_
