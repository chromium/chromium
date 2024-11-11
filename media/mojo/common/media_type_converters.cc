// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/subsample_entry.h"
#include "mojo/public/cpp/system/buffer.h"

namespace mojo {

// TODO(crbug.com/40468949): Stop using TypeConverters.

// static
media::mojom::DecryptConfigPtr
TypeConverter<media::mojom::DecryptConfigPtr, media::DecryptConfig>::Convert(
    const media::DecryptConfig& input) {
  media::mojom::DecryptConfigPtr mojo_decrypt_config(
      media::mojom::DecryptConfig::New());
  mojo_decrypt_config->key_id = input.key_id();
  mojo_decrypt_config->iv = input.iv();
  mojo_decrypt_config->subsamples = input.subsamples();
  mojo_decrypt_config->encryption_scheme = input.encryption_scheme();
  mojo_decrypt_config->encryption_pattern = input.encryption_pattern();

  return mojo_decrypt_config;
}

// static
std::unique_ptr<media::DecryptConfig>
TypeConverter<std::unique_ptr<media::DecryptConfig>,
              media::mojom::DecryptConfigPtr>::
    Convert(const media::mojom::DecryptConfigPtr& input) {
  return std::make_unique<media::DecryptConfig>(
      input->encryption_scheme, input->key_id, input->iv, input->subsamples,
      input->encryption_pattern);
}

// static
media::mojom::DecoderBufferSideDataPtr TypeConverter<
    media::mojom::DecoderBufferSideDataPtr,
    media::DecoderBufferSideData>::Convert(const media::DecoderBufferSideData&
                                               input) {
  media::mojom::DecoderBufferSideDataPtr mojo_side_data(
      media::mojom::DecoderBufferSideData::New());
  if (!input.alpha_data.empty()) {
    mojo_side_data->alpha_data.assign(input.alpha_data.begin(),
                                      input.alpha_data.end());
  }
  mojo_side_data->spatial_layers = input.spatial_layers;
  mojo_side_data->secure_handle = input.secure_handle;
  mojo_side_data->front_discard = input.discard_padding.first;
  mojo_side_data->back_discard = input.discard_padding.second;

  // Note: `next_audio_config` and `next_video_config` are intentionally not
  // serialized here since they are only set for EOS buffers.

  return mojo_side_data;
}

// static
std::unique_ptr<media::DecoderBufferSideData>
TypeConverter<std::unique_ptr<media::DecoderBufferSideData>,
              media::mojom::DecoderBufferSideDataPtr>::
    Convert(const media::mojom::DecoderBufferSideDataPtr& input) {
  if (!input) {
    return nullptr;
  }

  auto side_data = std::make_unique<media::DecoderBufferSideData>();
  side_data->spatial_layers = input->spatial_layers;
  if (!input->alpha_data.empty()) {
    side_data->alpha_data =
        base::HeapArray<uint8_t>::CopiedFrom(input->alpha_data);
  }
  side_data->secure_handle = input->secure_handle;
  side_data->discard_padding.first = input->front_discard;
  side_data->discard_padding.second = input->back_discard;

  // Note: `next_audio_config` and `next_video_config` are intentionally not
  // deserialized here since they are only set for EOS buffers.

  return side_data;
}

// static
media::mojom::DecoderBufferPtr
TypeConverter<media::mojom::DecoderBufferPtr, media::DecoderBuffer>::Convert(
    const media::DecoderBuffer& input) {
  if (input.end_of_stream()) {
    auto eos = media::mojom::EosDecoderBuffer::New();
    if (input.next_config()) {
      const auto next_config = *input.next_config();
      if (const auto* ac =
              absl::get_if<media::AudioDecoderConfig>(&next_config)) {
        eos->next_config =
            media::mojom::DecoderBufferSideDataNextConfig::NewNextAudioConfig(
                *ac);
      } else {
        eos->next_config =
            media::mojom::DecoderBufferSideDataNextConfig::NewNextVideoConfig(
                absl::get<media::VideoDecoderConfig>(next_config));
      }
    }
    return media::mojom::DecoderBuffer::NewEos(std::move(eos));
  }

  auto data_buffer = media::mojom::DataDecoderBuffer::New();
  data_buffer->timestamp = input.timestamp();
  data_buffer->duration = input.duration();
  data_buffer->is_key_frame = input.is_key_frame();
  data_buffer->data_size = base::checked_cast<uint32_t>(input.size());
  if (input.has_side_data()) {
    data_buffer->side_data =
        media::mojom::DecoderBufferSideData::From(*input.side_data());
  }

  if (input.decrypt_config()) {
    data_buffer->decrypt_config =
        media::mojom::DecryptConfig::From(*input.decrypt_config());
  }

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return media::mojom::DecoderBuffer::NewData(std::move(data_buffer));
}

// static
scoped_refptr<media::DecoderBuffer>
TypeConverter<scoped_refptr<media::DecoderBuffer>,
              media::mojom::DecoderBufferPtr>::
    Convert(const media::mojom::DecoderBufferPtr& input) {
  if (input->is_eos()) {
    const auto& eos_buffer = input->get_eos();
    if (eos_buffer->next_config) {
      if (eos_buffer->next_config->is_next_audio_config()) {
        return media::DecoderBuffer::CreateEOSBuffer(
            eos_buffer->next_config->get_next_audio_config());
      } else if (eos_buffer->next_config->is_next_video_config()) {
        return media::DecoderBuffer::CreateEOSBuffer(
            eos_buffer->next_config->get_next_video_config());
      }
    }
    return media::DecoderBuffer::CreateEOSBuffer();
  }

  const auto& mojo_buffer = input->get_data();
  auto buffer = base::MakeRefCounted<media::DecoderBuffer>(
      base::strict_cast<size_t>(mojo_buffer->data_size));

  if (mojo_buffer->side_data) {
    buffer->set_side_data(
        mojo_buffer->side_data
            .To<std::unique_ptr<media::DecoderBufferSideData>>());
  }

  buffer->set_timestamp(mojo_buffer->timestamp);
  buffer->set_duration(mojo_buffer->duration);
  buffer->set_is_key_frame(mojo_buffer->is_key_frame);

  if (mojo_buffer->decrypt_config) {
    buffer->set_decrypt_config(
        mojo_buffer->decrypt_config
            .To<std::unique_ptr<media::DecryptConfig>>());
  }

  // TODO(dalecurtis): We intentionally do not deserialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return buffer;
}

// static
media::mojom::AudioBufferPtr
TypeConverter<media::mojom::AudioBufferPtr, media::AudioBuffer>::Convert(
    const media::AudioBuffer& input) {
  media::mojom::AudioBufferPtr buffer(media::mojom::AudioBuffer::New());
  buffer->sample_format = input.sample_format_;
  buffer->channel_layout = input.channel_layout();
  buffer->channel_count = input.channel_count();
  buffer->sample_rate = input.sample_rate();
  buffer->frame_count = input.frame_count();
  buffer->end_of_stream = input.end_of_stream();
  buffer->timestamp = input.timestamp();

  if (input.data_) {
    // `input.data_->span()` refers to the whole memory buffer given to the
    // `media::AudioBuffer`.
    // `data_size()` refers to the amount of memory really used by the audio
    // data. The rest is padding, which we don't need to copy.
    DCHECK_GT(input.data_size(), 0u);
    DCHECK_GE(input.data_size(), input.data_->span().size());
    auto buffer_start = input.data_->span().begin();
    auto buffer_end = buffer_start + input.data_size();
    buffer->data.assign(buffer_start, buffer_end);
  }

  return buffer;
}

// static
scoped_refptr<media::AudioBuffer>
TypeConverter<scoped_refptr<media::AudioBuffer>, media::mojom::AudioBufferPtr>::
    Convert(const media::mojom::AudioBufferPtr& input) {
  if (input->end_of_stream)
    return media::AudioBuffer::CreateEOSBuffer();

  if (input->frame_count <= 0 ||
      static_cast<size_t>(input->sample_format) > media::kSampleFormatMax ||
      static_cast<size_t>(input->channel_layout) > media::CHANNEL_LAYOUT_MAX ||
      ChannelLayoutToChannelCount(input->channel_layout) !=
          input->channel_count) {
    LOG(ERROR) << "Receive an invalid audio buffer, replace it with EOS.";
    return media::AudioBuffer::CreateEOSBuffer();
  }

  if (IsBitstream(input->sample_format)) {
    uint8_t* data = input->data.data();
    return media::AudioBuffer::CopyBitstreamFrom(
        input->sample_format, input->channel_layout, input->channel_count,
        input->sample_rate, input->frame_count, &data, input->data.size(),
        input->timestamp);
  }

  // Setup channel pointers.  AudioBuffer::CopyFrom() will only use the first
  // one in the case of interleaved data.
  std::vector<const uint8_t*> channel_ptrs(input->channel_count, nullptr);
  const size_t size_per_channel = input->data.size() / input->channel_count;
  DCHECK_EQ(0u, input->data.size() % input->channel_count);
  for (int i = 0; i < input->channel_count; ++i)
    channel_ptrs[i] = input->data.data() + i * size_per_channel;

  return media::AudioBuffer::CopyFrom(
      input->sample_format, input->channel_layout, input->channel_count,
      input->sample_rate, input->frame_count, &channel_ptrs[0],
      input->timestamp);
}

}  // namespace mojo
