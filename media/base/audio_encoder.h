// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_ENCODER_H_
#define MEDIA_BASE_AUDIO_ENCODER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_codecs.h"
#include "media/base/audio_parameters.h"
#include "media/base/encoder_status.h"
#include "media/base/media_export.h"
#include "media/base/timestamp_constants.h"

namespace media {

// Defines a move-only wrapper to hold the encoded audio data.
struct MEDIA_EXPORT EncodedAudioBuffer {
  EncodedAudioBuffer();
  EncodedAudioBuffer(const AudioParameters& params,
                     base::HeapArray<uint8_t> data,
                     base::TimeTicks timestamp,
                     base::TimeDelta duration = media::kNoTimestamp);
  EncodedAudioBuffer(EncodedAudioBuffer&&);
  EncodedAudioBuffer& operator=(EncodedAudioBuffer&&);
  ~EncodedAudioBuffer();

  // The audio parameters the encoder used to encode the input audio. They may
  // differ from the original parameters given to the encoder initially, as the
  // encoder may convert the audio to a format more suitable for encoding.
  AudioParameters params;

  // The buffer containing the encoded data.
  base::HeapArray<uint8_t> encoded_data;

  // The capture time of the first sample of the current AudioBus, or a previous
  // AudioBus If this output was generated because of a call to Flush().
  base::TimeTicks timestamp;

  // The duration of the encoded samples, if they were decoded and played out.
  // A duration of media::kNoTimestamp means we don't know the duration or don't
  // care about it.
  base::TimeDelta duration;
};

// Defines an interface for audio encoders.
class MEDIA_EXPORT AudioEncoder {
 public:
  enum class OpusSignal { kAuto, kMusic, kVoice };
  enum class OpusApplication { kVoip, kAudio, kLowDelay };
  struct MEDIA_EXPORT OpusOptions {
    base::TimeDelta frame_duration;
    OpusSignal signal;
    OpusApplication application;
    unsigned int complexity;
    unsigned int packet_loss_perc;
    bool use_in_band_fec;
    bool use_dtx;
  };

  enum class AacOutputFormat { AAC, ADTS };
  struct MEDIA_EXPORT AacOptions {
    AacOutputFormat format;
  };

  enum class BitrateMode { kVariable, kConstant };

  struct MEDIA_EXPORT Options {
    Options();
    Options(const Options&);
    ~Options();

    AudioCodec codec;

    std::optional<int> bitrate;

    int channels;

    int sample_rate;

    std::optional<BitrateMode> bitrate_mode;

    std::optional<OpusOptions> opus;
    std::optional<AacOptions> aac;
  };

  // A sequence of codec specific bytes, commonly known as extradata.
  using CodecDescription = std::vector<uint8_t>;

  // Signature of the callback invoked to provide the encoded audio data. It is
  // invoked on the same sequence on which EncodeAudio() is called.
  using OutputCB =
      base::RepeatingCallback<void(EncodedAudioBuffer output,
                                   std::optional<CodecDescription>)>;

  // Signature of the callback to report errors.
  using EncoderStatusCB = base::OnceCallback<void(EncoderStatus error)>;

  AudioEncoder();
  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;
  virtual ~AudioEncoder();

  // Initializes an AudioEncoder with the given input option, executing
  // the |done_cb| upon completion. |output_cb| is called for each encoded audio
  // chunk.
  //
  // No AudioEncoder calls should be made before |done_cb| is executed.
  virtual void Initialize(const Options& options,
                          OutputCB output_cb,
                          EncoderStatusCB done_cb) = 0;

  // Requests contents of |audio_bus| to be encoded.
  // |capture_time| is a media time at the end of the audio piece in the
  // |audio_bus|.
  //
  // |done_cb| is called upon encode completion and can possible convey an
  // encoding error. It doesn't depend on future call to encoder's methods.
  // |done_cb| will not be called from within this method.
  //
  // After the input, or several inputs, are encoded the encoder calls
  // |output_cb|.
  // |output_cb| may be called before or after |done_cb|,
  // including before Encode() returns.
  virtual void Encode(std::unique_ptr<AudioBus> audio_bus,
                      base::TimeTicks capture_time,
                      EncoderStatusCB done_cb) = 0;

  // Some encoders may choose to buffer audio frames before they encode them.
  // Requests all outputs for already encoded frames to be
  // produced via |output_cb| and calls |done_cb| after that.
  virtual void Flush(EncoderStatusCB done_cb) = 0;

  // Normally AudioEncoder implementations aren't supposed to call OutputCB and
  // EncoderStatusCB directly from inside any of AudioEncoder's methods.
  // This method tells AudioEncoder that all callbacks can be called directly
  // from within its methods. It saves extra thread hops if it's known that
  // all callbacks already point to a task runner different from
  // the current one.
  virtual void DisablePostedCallbacks();

 protected:
  OutputCB BindCallbackToCurrentLoopIfNeeded(OutputCB&& callback);
  EncoderStatusCB BindCallbackToCurrentLoopIfNeeded(EncoderStatusCB&& callback);

  bool post_callbacks_ = true;

  Options options_;

  OutputCB output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

using AudioEncoderConfig = AudioEncoder::Options;

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_ENCODER_H_
