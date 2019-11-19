// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_adapter.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamAdapter::MojoDemuxerStreamAdapter(
    mojo::PendingRemote<mojom::DemuxerStream> demuxer_stream,
    const base::Closure& stream_ready_cb)
    : demuxer_stream_(std::move(demuxer_stream)),
      stream_ready_cb_(stream_ready_cb),
      type_(UNKNOWN) {
  DVLOG(1) << __func__;
  demuxer_stream_->Initialize(base::Bind(
      &MojoDemuxerStreamAdapter::OnStreamReady, weak_factory_.GetWeakPtr()));
}

MojoDemuxerStreamAdapter::~MojoDemuxerStreamAdapter() {
  DVLOG(1) << __func__;
}

void MojoDemuxerStreamAdapter::Read(ReadCB read_cb) {
  DVLOG(3) << __func__;
  // We shouldn't be holding on to a previous callback if a new Read() came in.
  DCHECK(!read_cb_);

  read_cb_ = std::move(read_cb);
  demuxer_stream_->Read(base::Bind(&MojoDemuxerStreamAdapter::OnBufferReady,
                                   weak_factory_.GetWeakPtr()));
}

bool MojoDemuxerStreamAdapter::IsReadPending() const {
  return !read_cb_.is_null();
}

AudioDecoderConfig MojoDemuxerStreamAdapter::audio_decoder_config() {
  DCHECK_EQ(type_, AUDIO);
  return audio_config_;
}

VideoDecoderConfig MojoDemuxerStreamAdapter::video_decoder_config() {
  DCHECK_EQ(type_, VIDEO);
  return video_config_;
}

DemuxerStream::Type MojoDemuxerStreamAdapter::type() const {
  return type_;
}

void MojoDemuxerStreamAdapter::EnableBitstreamConverter() {
  demuxer_stream_->EnableBitstreamConverter();
}

bool MojoDemuxerStreamAdapter::SupportsConfigChanges() {
  return true;
}

// TODO(xhwang): Pass liveness here.
void MojoDemuxerStreamAdapter::OnStreamReady(
    Type type,
    mojo::ScopedDataPipeConsumerHandle consumer_handle,
    const base::Optional<AudioDecoderConfig>& audio_config,
    const base::Optional<VideoDecoderConfig>& video_config) {
  DVLOG(1) << __func__;
  DCHECK_EQ(UNKNOWN, type_);
  DCHECK(consumer_handle.is_valid());

  type_ = type;

  mojo_decoder_buffer_reader_.reset(
      new MojoDecoderBufferReader(std::move(consumer_handle)));

  UpdateConfig(std::move(audio_config), std::move(video_config));

  stream_ready_cb_.Run();
}

void MojoDemuxerStreamAdapter::OnBufferReady(
    Status status,
    mojom::DecoderBufferPtr buffer,
    const base::Optional<AudioDecoderConfig>& audio_config,
    const base::Optional<VideoDecoderConfig>& video_config) {
  DVLOG(3) << __func__;
  DCHECK(read_cb_);
  DCHECK_NE(type_, UNKNOWN);

  if (status == kConfigChanged) {
    UpdateConfig(std::move(audio_config), std::move(video_config));
    std::move(read_cb_).Run(kConfigChanged, nullptr);
    return;
  }

  if (status == kAborted) {
    std::move(read_cb_).Run(kAborted, nullptr);
    return;
  }

  DCHECK_EQ(status, kOk);
  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(buffer), base::BindOnce(&MojoDemuxerStreamAdapter::OnBufferRead,
                                        weak_factory_.GetWeakPtr()));
}

void MojoDemuxerStreamAdapter::OnBufferRead(
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    std::move(read_cb_).Run(kAborted, nullptr);
    return;
  }

  std::move(read_cb_).Run(kOk, buffer);
}

void MojoDemuxerStreamAdapter::UpdateConfig(
    const base::Optional<AudioDecoderConfig>& audio_config,
    const base::Optional<VideoDecoderConfig>& video_config) {
  DCHECK_NE(type_, UNKNOWN);

  switch(type_) {
    case AUDIO:
      DCHECK(audio_config && !video_config);
      audio_config_ = audio_config.value();
      break;
    case VIDEO:
      DCHECK(video_config && !audio_config);
      video_config_ = video_config.value();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace media
