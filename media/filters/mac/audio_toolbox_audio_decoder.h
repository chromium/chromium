// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_DECODER_H_
#define MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_DECODER_H_

#include <memory>

#include <AudioToolbox/AudioToolbox.h>

#include "base/apple/scoped_typeref.h"
#include "base/memory/free_deleter.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"

namespace media {

class AudioBufferMemoryPool;
class AudioDiscardHelper;
class MediaLog;

// Audio decoder based on macOS's AudioToolbox API. The AudioToolbox
// API is required to decode codecs that aren't supported by Chromium.
class MEDIA_EXPORT AudioToolboxAudioDecoder : public AudioDecoder {
 public:
  explicit AudioToolboxAudioDecoder(std::unique_ptr<MediaLog> media_log);

  AudioToolboxAudioDecoder(const AudioToolboxAudioDecoder&) = delete;
  AudioToolboxAudioDecoder& operator=(const AudioToolboxAudioDecoder&) = delete;

  ~AudioToolboxAudioDecoder() override;

  // AudioDecoder implementation.
  AudioDecoderType GetDecoderType() const override;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  bool NeedsBitstreamConversion() const override;

 private:
  struct ScopedAudioConverterRefTraits {
    static AudioConverterRef InvalidValue() { return nullptr; }
    static AudioConverterRef Retain(AudioConverterRef converter);
    static void Release(AudioConverterRef converter);
  };
  using ScopedAudioConverterRef =
      base::apple::ScopedTypeRef<AudioConverterRef,
                                 ScopedAudioConverterRefTraits>;

  bool CreateDecoder(const AudioDecoderConfig& config);

  std::unique_ptr<MediaLog> media_log_;

  // "Converter" for turning encoded samples into raw audio.
  ScopedAudioConverterRef decoder_;

  // Actual channel count and layout from decoder, may be different than config.
  uint32_t channel_count_ = 0u;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_UNSUPPORTED;

  // Actual sample rate from the decoder, may be different than config.
  uint32_t sample_rate_ = 0u;

  // Callback that delivers output frames.
  OutputCB output_cb_;

  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  // Pool which helps avoid thrashing memory when returning audio buffers.
  scoped_refptr<AudioBufferMemoryPool> pool_;

  // Staging structures for receiving decoded data.
  std::unique_ptr<AudioBus> output_bus_;
  std::unique_ptr<AudioBufferList, base::FreeDeleter> output_buffer_list_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_MAC_AUDIO_TOOLBOX_AUDIO_DECODER_H_
