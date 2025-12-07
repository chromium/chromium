// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/validation_utils.h"

namespace media {

std::unique_ptr<media::DecryptConfig> ValidateAndConvertMojoDecryptConfig(
    media::mojom::DecryptConfigPtr decrypt_config) {
  CHECK(decrypt_config);
  if (decrypt_config->encryption_scheme ==
      media::EncryptionScheme::kUnencrypted) {
    // The DecryptConfig constructor has a DCHECK() that rejects
    // EncryptionScheme::kUnencrypted.
    return nullptr;
  }
  if (decrypt_config->key_id.empty()) {
    return nullptr;
  }
  if (decrypt_config->iv.size() !=
      static_cast<size_t>(media::DecryptConfig::kDecryptionKeySize)) {
    return nullptr;
  }
  if (decrypt_config->encryption_scheme != media::EncryptionScheme::kCbcs &&
      decrypt_config->encryption_pattern.has_value()) {
    return nullptr;
  }
  return std::make_unique<media::DecryptConfig>(
      decrypt_config->encryption_scheme, std::move(decrypt_config->key_id),
      std::move(decrypt_config->iv), std::move(decrypt_config->subsamples),
      decrypt_config->encryption_pattern);
}

std::unique_ptr<media::DecoderBufferSideData>
ValidateAndConvertMojoDecoderBufferSideData(
    media::mojom::DecoderBufferSideDataPtr side_data) {
  if (!side_data) {
    return nullptr;
  }
  constexpr size_t kMaxSpatialLayers = 3;
  if (side_data->spatial_layers.size() > kMaxSpatialLayers) {
    return nullptr;
  }
  auto media_side_data = std::make_unique<media::DecoderBufferSideData>();
  media_side_data->spatial_layers = side_data->spatial_layers;
  if (!side_data->alpha_data.empty()) {
    media_side_data->alpha_data =
        base::HeapArray<uint8_t>::CopiedFrom(side_data->alpha_data);
  }
  media_side_data->secure_handle = side_data->secure_handle;
  media_side_data->discard_padding.first = side_data->front_discard;
  media_side_data->discard_padding.second = side_data->back_discard;
  return media_side_data;
}

scoped_refptr<media::DecoderBuffer> ValidateAndConvertMojoDecoderBuffer(
    media::mojom::DecoderBufferPtr decoder_buffer) {
  if (!decoder_buffer) {
    return nullptr;
  }

  if (decoder_buffer->is_eos()) {
    const auto& eos_buffer = decoder_buffer->get_eos();
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

  const auto& mojo_buffer = decoder_buffer->get_data();
  CHECK(!!mojo_buffer);

  if (mojo_buffer->duration != media::kNoTimestamp &&
      (mojo_buffer->duration < base::TimeDelta() ||
       mojo_buffer->duration == media::kInfiniteDuration)) {
    return nullptr;
  }

  std::unique_ptr<media::DecryptConfig> decrypt_config;
  if (mojo_buffer->decrypt_config) {
    decrypt_config = ValidateAndConvertMojoDecryptConfig(
        std::move(mojo_buffer->decrypt_config));
    if (!decrypt_config) {
      return nullptr;
    }
  }

  std::unique_ptr<media::DecoderBufferSideData> side_data;
  if (mojo_buffer->side_data) {
    side_data = ValidateAndConvertMojoDecoderBufferSideData(
        std::move(mojo_buffer->side_data));
    if (!side_data) {
      return nullptr;
    }
  }

  auto media_buffer = base::MakeRefCounted<media::DecoderBuffer>(
      base::strict_cast<size_t>(mojo_buffer->data_size));
  if (side_data) {
    media_buffer->set_side_data(std::move(side_data));
  }
  media_buffer->set_timestamp(mojo_buffer->timestamp);
  media_buffer->set_duration(mojo_buffer->duration);
  media_buffer->set_is_key_frame(mojo_buffer->is_key_frame);
  if (decrypt_config) {
    media_buffer->set_decrypt_config(std::move(decrypt_config));
  }
  return media_buffer;
}

}  // namespace media
