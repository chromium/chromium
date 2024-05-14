// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/clients/mojo_demuxer_stream_impl.h"

#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
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
  std::optional<AudioDecoderConfig> audio_config;
  std::optional<VideoDecoderConfig> video_config;
  if (stream_->type() == Type::AUDIO) {
    audio_config = stream_->audio_decoder_config();
  } else if (stream_->type() == Type::VIDEO) {
    video_config = stream_->video_decoder_config();
  } else {
    NOTREACHED_IN_MIGRATION() << "Unsupported stream type: " << stream_->type();
    return;
  }

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      GetDefaultDecoderBufferConverterCapacity(stream_->type()),
      &remote_consumer_handle);

  std::move(callback).Run(stream_->type(), std::move(remote_consumer_handle),
                          audio_config, video_config);
}

void MojoDemuxerStreamImpl::Read(uint32_t count, ReadCallback callback) {
  DVLOG(3) << __func__ << ": count=" << count;
  stream_->Read(
      count, base::BindOnce(&MojoDemuxerStreamImpl::OnBufferReady,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MojoDemuxerStreamImpl::EnableBitstreamConverter() {
  stream_->EnableBitstreamConverter();
}

void MojoDemuxerStreamImpl::OnBufferReady(
    ReadCallback callback,
    Status status,
    media::DemuxerStream::DecoderBufferVector buffers) {
  std::optional<AudioDecoderConfig> audio_config;
  std::optional<VideoDecoderConfig> video_config;
  DVLOG(3) << __func__
           << ": status=" << ::media::DemuxerStream::GetStatusName(status)
           << ", buffers.size=" << buffers.size();

  if (status == Status::kConfigChanged) {
    // To simply the config change handling on renderer(receiver) side, prefer
    // to send out buffers before config change happens. For FFmpegDemuxer, it
    // doesn't make config change. For ChunkDemuxer, it send out buffer before
    // confige change happen. |buffers| is empty at this point.
    DCHECK(buffers.empty());

    // Send the config change so our client can read it once it parses the
    // Status obtained via Run() below.
    if (stream_->type() == Type::AUDIO) {
      audio_config = stream_->audio_decoder_config();
    } else if (stream_->type() == Type::VIDEO) {
      video_config = stream_->video_decoder_config();
    } else {
      NOTREACHED_IN_MIGRATION()
          << "Unsupported config change encountered for type: "
          << stream_->type();
    }
    std::move(callback).Run(Status::kConfigChanged, {}, audio_config,
                            video_config);
    return;
  }

  if (status == Status::kAborted) {
    std::move(callback).Run(Status::kAborted, {}, audio_config, video_config);
    return;
  }

  DCHECK_EQ(status, Status::kOk);

  std::vector<mojom::DecoderBufferPtr> output_mojo_buffers;
  for (auto& buffer : buffers) {
    mojom::DecoderBufferPtr mojo_buffer =
        mojo_decoder_buffer_writer_->WriteDecoderBuffer(std::move(buffer));
    if (!mojo_buffer) {
      std::move(callback).Run(Status::kAborted, {}, audio_config, video_config);
      return;
    }
    output_mojo_buffers.emplace_back(std::move(mojo_buffer));
  }

  // TODO(dalecurtis): Once we can write framed data to the DataPipe, fill via
  // the producer handle and then read more to keep the pipe full.  Waiting for
  // space can be accomplished using an AsyncWaiter.
  std::move(callback).Run(status, std::move(output_mojo_buffers), audio_config,
                          video_config);
}

}  // namespace media
