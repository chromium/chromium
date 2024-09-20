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
media::mojom::DecoderBufferSideDataPtr
TypeConverter<media::mojom::DecoderBufferSideDataPtr,
              std::optional<media::DecoderBufferSideData>>::
    Convert(const std::optional<media::DecoderBufferSideData>& input) {
  if (!input.has_value()) {
    return nullptr;
  }
  media::mojom::DecoderBufferSideDataPtr mojo_side_data(
      media::mojom::DecoderBufferSideData::New());
  mojo_side_data->alpha_data = input->alpha_data;
  mojo_side_data->spatial_layers = input->spatial_layers;
  mojo_side_data->secure_handle = input->secure_handle;
  mojo_side_data->front_discard = input->discard_padding.first;
  mojo_side_data->back_discard = input->discard_padding.second;

  // Note: `next_audio_config` and `next_video_config` are intentionally not
  // serialized here since they are only set for EOS buffers.
  // TODO(crbug.com/366491584): Remove this note once we've switched EOS
  // handling to use API calls instead of special buffer types.

  return mojo_side_data;
}

// static
std::optional<media::DecoderBufferSideData>
TypeConverter<std::optional<media::DecoderBufferSideData>,
              media::mojom::DecoderBufferSideDataPtr>::
    Convert(const media::mojom::DecoderBufferSideDataPtr& input) {
  if (!input) {
    return std::nullopt;
  }
  auto side_data = std::make_optional<media::DecoderBufferSideData>(
      media::DecoderBufferSideData());
  side_data->alpha_data = input->alpha_data;
  side_data->spatial_layers = input->spatial_layers;
  side_data->secure_handle = input->secure_handle;
  side_data->discard_padding.first = input->front_discard;
  side_data->discard_padding.second = input->back_discard;

  // Note: `next_audio_config` and `next_video_config` are intentionally not
  // deserialized here since they are only set for EOS buffers.
  // TODO(crbug.com/366491584): Remove this note once we've switched EOS
  // handling to use API calls instead of special buffer types.

  return side_data;
}

// static
media::mojom::DecoderBufferPtr
TypeConverter<media::mojom::DecoderBufferPtr, media::DecoderBuffer>::Convert(
    const media::DecoderBuffer& input) {
  media::mojom::DecoderBufferPtr mojo_buffer(
      media::mojom::DecoderBuffer::New());
  if (input.end_of_stream()) {
    mojo_buffer->is_end_of_stream = true;
    // TODO(crbug.com/366491584): This should be handled via a new API.
    if (input.next_config()) {
      mojo_buffer->side_data = media::mojom::DecoderBufferSideData::New();
      const auto next_config = *input.next_config();
      if (const auto* ac =
              absl::get_if<media::AudioDecoderConfig>(&next_config)) {
        mojo_buffer->side_data->next_config =
            media::mojom::DecoderBufferSideDataNextConfig::NewNextAudioConfig(
                *ac);
      } else {
        mojo_buffer->side_data->next_config =
            media::mojom::DecoderBufferSideDataNextConfig::NewNextVideoConfig(
                absl::get<media::VideoDecoderConfig>(next_config));
      }
    }
    return mojo_buffer;
  }

  mojo_buffer->is_end_of_stream = false;
  mojo_buffer->timestamp = input.timestamp();
  mojo_buffer->duration = input.duration();
  mojo_buffer->is_key_frame = input.is_key_frame();
  mojo_buffer->data_size = base::checked_cast<uint32_t>(input.size());
  mojo_buffer->side_data =
      media::mojom::DecoderBufferSideData::From(input.side_data());

  if (input.decrypt_config()) {
    mojo_buffer->decrypt_config =
        media::mojom::DecryptConfig::From(*input.decrypt_config());
  }

  // TODO(dalecurtis): We intentionally do not serialize the data section of
  // the DecoderBuffer here; this must instead be done by clients via their
  // own DataPipe.  See http://crbug.com/432960

  return mojo_buffer;
}

// static
scoped_refptr<media::DecoderBuffer>
TypeConverter<scoped_refptr<media::DecoderBuffer>,
              media::mojom::DecoderBufferPtr>::
    Convert(const media::mojom::DecoderBufferPtr& input) {
  if (input->is_end_of_stream) {
    // TODO(crbug.com/366491584): This should be handled via a new API.
    if (input->side_data) {
      if (input->side_data->next_config &&
          input->side_data->next_config->is_next_audio_config()) {
        return media::DecoderBuffer::CreateEOSBuffer(
            input->side_data->next_config->get_next_audio_config());
      } else if (input->side_data->next_config &&
                 input->side_data->next_config->is_next_video_config()) {
        return media::DecoderBuffer::CreateEOSBuffer(
            input->side_data->next_config->get_next_video_config());
      } else {
        DLOG(ERROR) << "An AudioDecoderConfig or VideoDecoderConfig must be "
                       "present for an EOS buffer if side data exists.";
        return nullptr;
      }
    }
    return media::DecoderBuffer::CreateEOSBuffer();
  } else if (input->side_data && input->side_data->next_config) {
    DLOG(ERROR) << "AudioDecoderConfig or VideoDecoderConfig must not be "
                   "present for non-EOS buffers.";
    return nullptr;
  }

  scoped_refptr<media::DecoderBuffer> buffer(
      new media::DecoderBuffer(base::strict_cast<size_t>(input->data_size)));

  if (input->side_data) {
    buffer->set_side_data(
        input->side_data.To<std::optional<media::DecoderBufferSideData>>());
  }

  buffer->set_timestamp(input->timestamp);
  buffer->set_duration(input->duration);
  buffer->set_is_key_frame(input->is_key_frame);

  if (input->decrypt_config) {
    buffer->set_decrypt_config(
        input->decrypt_config.To<std::unique_ptr<media::DecryptConfig>>());
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
