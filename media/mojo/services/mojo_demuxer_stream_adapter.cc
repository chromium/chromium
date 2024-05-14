// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_demuxer_stream_adapter.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_decoder_buffer_converter.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace media {

MojoDemuxerStreamAdapter::MojoDemuxerStreamAdapter(
    mojo::PendingRemote<mojom::DemuxerStream> demuxer_stream,
    base::OnceClosure stream_ready_cb)
    : demuxer_stream_(std::move(demuxer_stream)),
      stream_ready_cb_(std::move(stream_ready_cb)) {
  DVLOG(1) << __func__;
  demuxer_stream_->Initialize(base::BindOnce(
      &MojoDemuxerStreamAdapter::OnStreamReady, weak_factory_.GetWeakPtr()));
}

MojoDemuxerStreamAdapter::~MojoDemuxerStreamAdapter() {
  DVLOG(1) << __func__;
}

void MojoDemuxerStreamAdapter::Read(uint32_t count, ReadCB read_cb) {
  DVLOG(3) << __func__;
  // We shouldn't be holding on to a previous callback if a new Read() came in.
  DCHECK(!read_cb_);

  read_cb_ = std::move(read_cb);
  demuxer_stream_->Read(count,
                        base::BindOnce(&MojoDemuxerStreamAdapter::OnBufferReady,
                                       weak_factory_.GetWeakPtr()));
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
    const std::optional<AudioDecoderConfig>& audio_config,
    const std::optional<VideoDecoderConfig>& video_config) {
  DVLOG(1) << __func__;
  DCHECK_EQ(UNKNOWN, type_);
  DCHECK(consumer_handle.is_valid());

  type_ = type;

  mojo_decoder_buffer_reader_ =
      std::make_unique<MojoDecoderBufferReader>(std::move(consumer_handle));

  UpdateConfig(std::move(audio_config), std::move(video_config));

  std::move(stream_ready_cb_).Run();
}

void MojoDemuxerStreamAdapter::OnBufferReady(
    Status status,
    std::vector<mojom::DecoderBufferPtr> batch_buffers,
    const std::optional<AudioDecoderConfig>& audio_config,
    const std::optional<VideoDecoderConfig>& video_config) {
  DVLOG(3) << __func__
           << ": status=" << ::media::DemuxerStream::GetStatusName(status)
           << ", batch_buffers.size=" << batch_buffers.size();
  DCHECK(read_cb_);
  DCHECK_NE(type_, UNKNOWN);

  if (status == kConfigChanged) {
    UpdateConfig(std::move(audio_config), std::move(video_config));
    std::move(read_cb_).Run(kConfigChanged, {});
    return;
  }

  if (status == kAborted) {
    std::move(read_cb_).Run(kAborted, {});
    return;
  }

  DCHECK_EQ(status, kOk);
  DCHECK_GT(batch_buffers.size(), 0u);

  status_ = status;
  actual_read_count_ = batch_buffers.size();

  batch_buffers_ = std::move(batch_buffers);
  mojo_decoder_buffer_reader_->ReadDecoderBuffer(
      std::move(batch_buffers_[0]),
      base::BindOnce(&MojoDemuxerStreamAdapter::OnBufferRead,
                     weak_factory_.GetWeakPtr()));
}

void MojoDemuxerStreamAdapter::OnBufferRead(
    scoped_refptr<DecoderBuffer> buffer) {
  if (!buffer) {
    DVLOG(1) << __func__ << ": null buffer";
    buffer_queue_.clear();
    std::move(read_cb_).Run(kAborted, {});
    return;
  }
  buffer_queue_.push_back(buffer);

  if (buffer_queue_.size() < actual_read_count_) {
    int next_read_index = buffer_queue_.size();
    // The `mojo_decoder_buffer_reader_` will run callback(OnBufferRead()) after
    // reading a buffer from data pipe, in order to avoid reentrance
    // OnBufferRead() and potential stack overflow , we use
    // base::BindPostTaskToCurrentDefault here. Each callback will correspond
    // one buffer until all buffers have been read.
    mojo_decoder_buffer_reader_->ReadDecoderBuffer(
        std::move(batch_buffers_[next_read_index]),
        base::BindPostTaskToCurrentDefault(
            base::BindOnce(&MojoDemuxerStreamAdapter::OnBufferRead,
                           weak_factory_.GetWeakPtr())));
    return;
  }

  DCHECK_EQ(buffer_queue_.size(), actual_read_count_);
  actual_read_count_ = 0;
  DemuxerStream::DecoderBufferVector buffer_queue;
  buffer_queue_.swap(buffer_queue);
  std::move(read_cb_).Run(status_, std::move(buffer_queue));
}

void MojoDemuxerStreamAdapter::UpdateConfig(
    const std::optional<AudioDecoderConfig>& audio_config,
    const std::optional<VideoDecoderConfig>& video_config) {
  DCHECK_NE(type_, Type::UNKNOWN);
  std::string old_decoder_config_str;

  switch(type_) {
    case AUDIO:
      DCHECK(audio_config && !video_config);
      old_decoder_config_str = audio_config_.AsHumanReadableString();
      audio_config_ = audio_config.value();
      TRACE_EVENT_INSTANT2(
          "media", "MojoDemuxerStreamAdapter.UpdateConfig.Audio",
          TRACE_EVENT_SCOPE_THREAD, "CurrentConfig", old_decoder_config_str,
          "NewConfig", audio_config_.AsHumanReadableString());
      break;
    case VIDEO:
      DCHECK(video_config && !audio_config);
      old_decoder_config_str = video_config_.AsHumanReadableString();
      video_config_ = video_config.value();
      TRACE_EVENT_INSTANT2(
          "media", "MojoDemuxerStreamAdapter.UpdateConfig.Video",
          TRACE_EVENT_SCOPE_THREAD, "CurrentConfig", old_decoder_config_str,
          "NewConfig", video_config_.AsHumanReadableString());
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

}  // namespace media
