// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FLAC_AUDIO_HANDLER_H_
#define MEDIA_AUDIO_FLAC_AUDIO_HANDLER_H_

#include <cstddef>
#include <memory>
#include <string_view>

#include "media/audio/audio_handler.h"
#include "media/base/media_export.h"
#include "third_party/flac/include/FLAC/stream_decoder.h"

namespace media {

class AudioBus;
class AudioFifo;

class FlacStreamDecoderDeleter {
 public:
  void operator()(FLAC__StreamDecoder* ptr) {
    FLAC__stream_decoder_delete(ptr);
  }
};

// This class provides the input from flac file format.
class MEDIA_EXPORT FlacAudioHandler : public AudioHandler {
 public:
  explicit FlacAudioHandler(std::string_view data);
  FlacAudioHandler(const FlacAudioHandler&) = delete;
  FlacAudioHandler& operator=(const FlacAudioHandler&) = delete;
  ~FlacAudioHandler() override;

  // AudioHandler:
  bool Initialize() override;
  int GetNumChannels() const override;
  int GetSampleRate() const override;
  base::TimeDelta GetDuration() const override;
  bool AtEnd() const override;
  bool CopyTo(AudioBus* bus, size_t* frames_written) override;
  void Reset() override;

  int total_frames_for_testing() const { return total_frames_; }

 private:
  // Callbacks for the `decoder_`.
  static FLAC__StreamDecoderReadStatus ReadCallback(
      const FLAC__StreamDecoder* decoder,
      FLAC__byte buffer[],
      size_t* bytes,
      void* client_data);
  static FLAC__StreamDecoderWriteStatus WriteCallback(
      const FLAC__StreamDecoder* decoder,
      const FLAC__Frame* frame,
      const FLAC__int32* const buffer[],
      void* client_data);
  static void MetaCallback(const FLAC__StreamDecoder* decoder,
                           const FLAC__StreamMetadata* metadata,
                           void* client_data);
  static void ErrorCallback(const FLAC__StreamDecoder* decoder,
                            FLAC__StreamDecoderErrorStatus status,
                            void* client_data);

  bool is_initialized() const { return !!fifo_; }

  // Internal callbacks.
  FLAC__StreamDecoderReadStatus ReadCallbackInternal(FLAC__byte buffer[],
                                                     size_t* bytes);
  FLAC__StreamDecoderWriteStatus WriteCallbackInternal(
      const FLAC__Frame* frame,
      const FLAC__int32* const buffer[]);
  void MetaCallbackInternal(const FLAC__StreamMetadata* metadata);
  void ErrorCallbackInternal();

  // Check if the metadata fecthed in `MetaCallback()` is valid or not. This
  // function will check the `num_channels_`, `bits_per_sample_`, `sample_rate_`
  // and `total_frames_`.
  // Note: We only consider 16 bits per second of audio data currently.
  bool AreParamsValid() const;

  const std::string_view flac_data_;
  const std::unique_ptr<FLAC__StreamDecoder, FlacStreamDecoderDeleter> decoder_;

  // To transfer decoded PCM samples from `WriteCallback` to `CopyTo`.
  std::unique_ptr<AudioFifo> fifo_;

  // The PCM buffer received in `WriteCallback()` is copied into `bus_`.
  std::unique_ptr<AudioBus> bus_;

  // Start to read `flac_data_` from `cursor_` position.
  size_t cursor_ = 0u;

  int num_channels_ = 0;
  int sample_rate_ = 0;
  int bits_per_sample_ = 0;

  // Equal to the total number of samples per channel.
  uint64_t total_frames_ = 0u;

  // Set to true if the error callback is called.
  bool has_error_ = false;

  // True if `WriteCallbackInternal` is called in a single call to
  // `FLAC__stream_decoder_process_single`.
  bool write_callback_called_ = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_FLAC_AUDIO_HANDLER_H_
