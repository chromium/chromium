// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_demuxer_stream_impl.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"

namespace media {

MojoDemuxerStreamImpl::MojoDemuxerStreamImpl(
    media::DemuxerStream* stream,
    mojo::PendingReceiver<mojom::DemuxerStream> receiver)
    : receiver_(this, std::move(receiver)), stream_(stream) {}

MojoDemuxerStreamImpl::~MojoDemuxerStreamImpl() = default;

// This is called when our DemuxerStreamClient has connected itself and is
// ready to receive messages.  Send an initial config and notify it that
// we are now ready for business.
void MojoDemuxerStreamImpl::Initialize(InitializeCallback callback) {
  DVLOG(2) << __func__;

  // Prepare the initial config.
  base::Optional<AudioDecoderConfig> audio_config;
  base::Optional<VideoDecoderConfig> video_config;
  if (stream_->type() == Type::AUDIO) {
    audio_config = stream_->audio_decoder_config();
  } else if (stream_->type() == Type::VIDEO) {
    video_config = stream_->video_decoder_config();
  } else {
    NOTREACHED() << "Unsupported stream type: " << stream_->type();
    return;
  }

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      GetDefaultDecoderBufferConverterCapacity(stream_->type()),
      &remote_consumer_handle);

  std::move(callback).Run(stream_->type(), std::move(remote_consumer_handle),
                          audio_config, video_config);
}

void MojoDemuxerStreamImpl::Read(ReadCallback callback) {
  stream_->Read(base::BindOnce(&MojoDemuxerStreamImpl::OnBufferReady,
                               weak_factory_.GetWeakPtr(),
                               base::Passed(&callback)));
}

void MojoDemuxerStreamImpl::EnableBitstreamConverter() {
  stream_->EnableBitstreamConverter();
}

void MojoDemuxerStreamImpl::OnBufferReady(ReadCallback callback,
                                          Status status,
                                          scoped_refptr<DecoderBuffer> buffer) {
  base::Optional<AudioDecoderConfig> audio_config;
  base::Optional<VideoDecoderConfig> video_config;

  if (status == Status::kConfigChanged) {
    DVLOG(2) << __func__ << ": ConfigChange!";
    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    if (stream_->type() == Type::AUDIO) {
      audio_config = stream_->audio_decoder_config();
    } else if (stream_->type() == Type::VIDEO) {
      video_config = stream_->video_decoder_config();
    } else {
      NOTREACHED() << "Unsupported config change encountered for type: "
                   << stream_->type();
    }

    std::move(callback).Run(Status::kConfigChanged, mojom::DecoderBufferPtr(),
                            audio_config, video_config);
    return;
  }

  if (status == Status::kAborted) {
    std::move(callback).Run(Status::kAborted, mojom::DecoderBufferPtr(),
                            audio_config, video_config);
    return;
  }

  DCHECK_EQ(status, Status::kOk);

  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
  if (!mojo_buffer) {
    std::move(callback).Run(Status::kAborted, mojom::DecoderBufferPtr(),
                            audio_config, video_config);
    return;
  }

  // TODO(dalecurtis): Once we can write framed data to the DataPipe, fill via
  // the producer handle and then read more to keep the pipe full.  Waiting for
  // space can be accomplished using an AsyncWaiter.
  std::move(callback).Run(status, std::move(mojo_buffer), audio_config,
                          video_config);
}

}  // namespace media
