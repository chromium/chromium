// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_AUDIO_DECODER_H_
#define MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_AUDIO_DECODER_H_

#include <mfidl.h>
#include <wrl/client.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"

namespace media {
class AudioBufferMemoryPool;
class AudioDiscardHelper;

// MFAudioDecoder is based on Window's MediaFoundation API. The MediaFoundation
// API is required to decode codecs that aren't supported by Chromium.
class MEDIA_EXPORT MediaFoundationAudioDecoder : public AudioDecoder {
 public:
  // Creates a MediaFoundationAudioDecoder if MediaFoundation is supported,
  // returns nullptr if not.
  static std::unique_ptr<MediaFoundationAudioDecoder> Create();

  MediaFoundationAudioDecoder();

  MediaFoundationAudioDecoder(const MediaFoundationAudioDecoder&) = delete;
  MediaFoundationAudioDecoder& operator=(const MediaFoundationAudioDecoder&) =
      delete;

  ~MediaFoundationAudioDecoder() override;

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
  // There are four states the decoder can be in:
  //
  // - kUninitialized: The decoder is not initialized.
  // - kNormal: This is the normal state. The decoder is idle and ready to
  //            decode input buffers, or is decoding an input buffer.
  // - kDecodeFinished: EOS buffer received, codec flushed and decode finished.
  //                    No further Decode() call should be made.
  // - kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kUninitialized -> kNormal:
  //     The decoder is successfully initialized and is ready to decode buffers.
  // kNormal -> kDecodeFinished:
  //     When buffer->end_of_stream() is true.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // (any state) -> kNormal:
  //     Any time Reset() is called.
  enum class DecoderState { kUninitialized, kNormal, kDecodeFinished, kError };

  bool CreateDecoder();
  bool ConfigureOutput();

  enum class OutputStatus { kSuccess, kNeedMoreInput, kStreamChange, kFailed };
  enum class PumpState { kNormal, kStreamChange };

  OutputStatus PumpOutput(PumpState pump_state);
  void ResetTimestampState();

  // Cached decoder config.
  AudioDecoderConfig config_;

  Microsoft::WRL::ComPtr<IMFTransform> decoder_;

  // Actual channel count and layout from decoder, may be different than config.
  uint32_t channel_count_ = 0u;
  ChannelLayout channel_layout_ = CHANNEL_LAYOUT_UNSUPPORTED;

  // Actual sample rate from the decoder, may be different than config.
  uint32_t sample_rate_ = 0u;

  // Output sample staging buffer
  Microsoft::WRL::ComPtr<IMFSample> output_sample_;

  // Callback that delivers output frames.
  OutputCB output_cb_;

  DecoderBuffer::TimeInfo current_buffer_time_info_;
  std::unique_ptr<AudioDiscardHelper> discard_helper_;

  // Pool which helps avoid thrashing memory when returning audio buffers.
  scoped_refptr<AudioBufferMemoryPool> pool_;

  // Used to rest timestamp_helper_ after Reset() is called
  bool has_reset_ = false;
};

}  // namespace media

#endif  // MEDIA_FILTERS_WIN_MEDIA_FOUNDATION_AUDIO_DECODER_H_
