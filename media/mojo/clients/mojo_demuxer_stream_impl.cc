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
    : receiver_(this, std::move(receiver)),
      type_(stream->type()),
      stream_(stream) {}

MojoDemuxerStreamImpl::~MojoDemuxerStreamImpl() = default;

void MojoDemuxerStreamImpl::SetStream(media::DemuxerStream* stream) {
  if (stream_ == stream && !waiting_for_track_change_) {
    return;
  }

  // Cancel any in-flight Read() callback from the previous stream.
  weak_factory_.InvalidateWeakPtrs();
  waiting_for_track_change_ = false;

  stream_ = stream;
  if (!stream_) {
    if (pending_read_cb_) {
      SendEos(std::move(pending_read_cb_));
    }
    return;
  }

  pending_config_change_ = true;
  if (bitstream_converter_enabled_) {
    stream_->EnableBitstreamConverter();
  }

  if (pending_read_cb_) {
    SendConfigChange(std::move(pending_read_cb_));
  }
}

void MojoDemuxerStreamImpl::PrepareForTrackChange() {
  weak_factory_.InvalidateWeakPtrs();
  stream_ = nullptr;
  waiting_for_track_change_ = true;
}

void MojoDemuxerStreamImpl::SendConfigChange(ReadCallback callback) {
  pending_config_change_ = false;
  std::optional<AudioDecoderConfig> audio_config;
  std::optional<VideoDecoderConfig> video_config;
  if (type_ == Type::AUDIO) {
    audio_config = stream_->audio_decoder_config();
  } else if (type_ == Type::VIDEO) {
    video_config = stream_->video_decoder_config();
  } else {
    NOTREACHED() << "Unsupported config change encountered for type: " << type_;
  }
  std::move(callback).Run(Status::kConfigChanged, {}, audio_config,
                          video_config);
}

void MojoDemuxerStreamImpl::SendEos(ReadCallback callback) {
  if (!mojo_decoder_buffer_writer_) {
    std::move(callback).Run(Status::kAborted, {}, std::nullopt, std::nullopt);
    return;
  }
  std::vector<mojom::DecoderBufferPtr> output_mojo_buffers;
  mojom::DecoderBufferPtr mojo_buffer =
      mojo_decoder_buffer_writer_->WriteDecoderBuffer(
          DecoderBuffer::CreateEOSBuffer());
  if (!mojo_buffer) {
    std::move(callback).Run(Status::kAborted, {}, std::nullopt, std::nullopt);
    return;
  }
  output_mojo_buffers.emplace_back(std::move(mojo_buffer));
  std::move(callback).Run(Status::kOk, std::move(output_mojo_buffers),
                          std::nullopt, std::nullopt);
}

// This is called when our DemuxerStreamClient has connected itself and is
// ready to receive messages.  Send an initial config and notify it that
// we are now ready for business.
void MojoDemuxerStreamImpl::Initialize(InitializeCallback callback) {
  DVLOG(2) << __func__;

  // Prepare the initial config.
  std::optional<AudioDecoderConfig> audio_config;
  std::optional<VideoDecoderConfig> video_config;
  if (type_ == Type::AUDIO) {
    audio_config = stream_->audio_decoder_config();
  } else if (type_ == Type::VIDEO) {
    video_config = stream_->video_decoder_config();
  } else {
    NOTREACHED() << "Unsupported stream type: " << type_;
  }

  mojo::ScopedDataPipeConsumerHandle remote_consumer_handle;
  mojo_decoder_buffer_writer_ = MojoDecoderBufferWriter::Create(
      GetDefaultDecoderBufferConverterCapacity(type_), &remote_consumer_handle);

  std::move(callback).Run(type_, std::move(remote_consumer_handle),
                          audio_config, video_config);
}

void MojoDemuxerStreamImpl::Read(uint32_t count, ReadCallback callback) {
  DVLOG(3) << __func__ << ": count=" << count;
  if (waiting_for_track_change_) {
    DCHECK(!pending_read_cb_);
    pending_read_cb_ = std::move(callback);
    return;
  }

  if (!stream_) {
    SendEos(std::move(callback));
    return;
  }

  if (pending_config_change_) {
    SendConfigChange(std::move(callback));
    return;
  }

  DCHECK(!pending_read_cb_);
  pending_read_cb_ = std::move(callback);
  stream_->Read(count, base::BindOnce(&MojoDemuxerStreamImpl::OnBufferReady,
                                      weak_factory_.GetWeakPtr()));
}

void MojoDemuxerStreamImpl::EnableBitstreamConverter() {
  bitstream_converter_enabled_ = true;
  if (stream_) {
    stream_->EnableBitstreamConverter();
  }
}

void MojoDemuxerStreamImpl::OnBufferReady(
    Status status,
    media::DemuxerStream::DecoderBufferVector buffers) {
  DCHECK(pending_read_cb_);
  ReadCallback callback = std::move(pending_read_cb_);

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
    SendConfigChange(std::move(callback));
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
