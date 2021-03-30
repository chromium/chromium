// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_ENCODER_H_
#define MEDIA_BASE_AUDIO_ENCODER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"
#include "media/base/status.h"

namespace media {

// Defines a move-only wrapper to hold the encoded audio data.
struct MEDIA_EXPORT EncodedAudioBuffer {
  EncodedAudioBuffer(const AudioParameters& params,
                     std::unique_ptr<uint8_t[]> data,
                     size_t size,
                     base::TimeTicks timestamp);
  EncodedAudioBuffer(EncodedAudioBuffer&&);
  ~EncodedAudioBuffer();

  // The audio parameters the encoder used to encode the input audio. They may
  // differ from the original parameters given to the encoder initially, as the
  // encoder may convert the audio to a format more suitable for encoding.
  const AudioParameters params;

  // The buffer containing the encoded data.
  std::unique_ptr<uint8_t[]> encoded_data;

  // The size of the encoded data in the above buffer. Note that this is not
  // necessarily equal to the capacity of the buffer. Some encoders allocate a
  // bigger buffer and fill it only with |encoded_data_size| data without
  // bothering to allocate another shrunk buffer and copy the data in, since the
  // number of encoded bytes may not be known in advance.
  const size_t encoded_data_size;

  // The capture time of the first sample of the current AudioBus, or a previous
  // AudioBus If this output was generated because of a call to Flush().
  const base::TimeTicks timestamp;
};

// Defines an interface for audio encoders.
class MEDIA_EXPORT AudioEncoder {
 public:
  struct MEDIA_EXPORT Options {
    Options();
    Options(const Options&);
    ~Options();

    base::Optional<int> bitrate;

    int channels;

    int sample_rate;
  };

  // A sequence of codec specific bytes, commonly known as extradata.
  using CodecDescription = std::vector<uint8_t>;

  // Signature of the callback invoked to provide the encoded audio data. It is
  // invoked on the same sequence on which EncodeAudio() is called.
  using OutputCB =
      base::RepeatingCallback<void(EncodedAudioBuffer output,
                                   base::Optional<CodecDescription>)>;

  // Signature of the callback to report errors.
  using StatusCB = base::OnceCallback<void(Status error)>;

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
                          StatusCB done_cb) = 0;

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
                      StatusCB done_cb) = 0;

  // Some encoders may choose to buffer audio frames before they encode them.
  // Requests all outputs for already encoded frames to be
  // produced via |output_cb| and calls |done_cb| after that.
  virtual void Flush(StatusCB done_cb) = 0;

 protected:
  Options options_;

  OutputCB output_cb_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_ENCODER_H_
