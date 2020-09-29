// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_ENCODER_H_
#define MEDIA_BASE_AUDIO_ENCODER_H_

#include <memory>

#include "base/callback.h"
#include "base/threading/thread_checker.h"
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

  // The capture time of the first sample of the current AudioBus.
  const base::TimeTicks timestamp;
};

// Defines an interface for audio encoders. Concrete encoders must implement the
// EncodeAudioImpl() function.
class MEDIA_EXPORT AudioEncoder {
 public:
  // Signature of the callback invoked to provide the encoded audio data. It is
  // invoked on the same thread on which EncodeAudio() is called. The utility
  // media::BindToCurrentLoop() can be used to create a callback that will be
  // invoked on the same thread it is constructed on.
  using EncodeCB = base::RepeatingCallback<void(EncodedAudioBuffer output)>;

  // Signature of the callback to report errors.
  using StatusCB = base::RepeatingCallback<void(Status error)>;

  // Constructs the encoder given the audio parameters of the input to this
  // encoder, and a callback to trigger to provide the encoded audio data.
  // |input_params| must be valid, and |encode_callback| and |status_callback|
  // must not be null callbacks. All calls to EncodeAudio() must happen on the
  // same thread (usually an encoder thread), but the encoder itself can be
  // constructed on any thread.
  AudioEncoder(const AudioParameters& input_params,
               EncodeCB encode_callback,
               StatusCB status_callback);
  AudioEncoder(const AudioEncoder&) = delete;
  AudioEncoder& operator=(const AudioEncoder&) = delete;
  virtual ~AudioEncoder();

  const AudioParameters& audio_input_params() const {
    return audio_input_params_;
  }

  // Performs various checks before calling EncodeAudioImpl() which does the
  // actual encoding.
  void EncodeAudio(const AudioBus& audio_bus, base::TimeTicks capture_time);

 protected:
  const EncodeCB& encode_callback() const { return encode_callback_; }
  const StatusCB& status_callback() const { return status_callback_; }
  base::TimeTicks last_capture_time() const { return last_capture_time_; }

  virtual void EncodeAudioImpl(const AudioBus& audio_bus,
                               base::TimeTicks capture_time) = 0;

  // Computes the timestamp of an AudioBus which has |num_frames| and was
  // captured at |capture_time|. This timestamp is the capture time of the first
  // sample in that AudioBus.
  base::TimeTicks ComputeTimestamp(int num_frames,
                                   base::TimeTicks capture_time) const;

 private:
  const AudioParameters audio_input_params_;

  const EncodeCB encode_callback_;

  const StatusCB status_callback_;

  // The capture time of the most recent |audio_bus| delivered to
  // EncodeAudio().
  base::TimeTicks last_capture_time_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_ENCODER_H_
