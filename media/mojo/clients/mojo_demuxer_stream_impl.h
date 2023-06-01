// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/mojom/demuxer_stream.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class DemuxerStream;
class MojoDecoderBufferWriter;

// This class wraps a media::DemuxerStream and exposes it as a
// mojom::DemuxerStream for use as a proxy from remote applications.
class MojoDemuxerStreamImpl : public mojom::DemuxerStream {
 public:
  // |stream| is the underlying DemuxerStream we are proxying for.
  // Note: |this| does not take ownership of |stream|.
  MojoDemuxerStreamImpl(media::DemuxerStream* stream,
                        mojo::PendingReceiver<mojom::DemuxerStream> receiver);

  MojoDemuxerStreamImpl(const MojoDemuxerStreamImpl&) = delete;
  MojoDemuxerStreamImpl& operator=(const MojoDemuxerStreamImpl&) = delete;

  ~MojoDemuxerStreamImpl() override;

  // mojom::DemuxerStream implementation.
  // InitializeCallback and ReadCallback are defined in
  // mojom::DemuxerStream.
  void Initialize(InitializeCallback callback) override;
  void Read(uint32_t count, ReadCallback callback) override;
  void EnableBitstreamConverter() override;

  // Sets an error handler that will be called if a connection error occurs on
  // the bound message pipe.
  void set_disconnect_handler(base::OnceClosure error_handler) {
    receiver_.set_disconnect_handler(std::move(error_handler));
  }

 private:
  using Type = media::DemuxerStream::Type;
  using Status = media::DemuxerStream::Status;

  void OnBufferReady(ReadCallback callback,
                     Status status,
                     media::DemuxerStream::DecoderBufferVector buffers);

  mojo::Receiver<mojom::DemuxerStream> receiver_;

  // See constructor.  We do not own |stream_|.
  raw_ptr<media::DemuxerStream, DanglingUntriaged> stream_;

  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  base::WeakPtrFactory<MojoDemuxerStreamImpl> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_
