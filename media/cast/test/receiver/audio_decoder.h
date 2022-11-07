// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_RECEIVER_AUDIO_DECODER_H_
#define MEDIA_CAST_TEST_RECEIVER_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/constants.h"

namespace media {
namespace cast {

struct EncodedFrame;

class AudioDecoder {
 public:
  // Callback passed to DecodeFrame, to deliver decoded audio data from the
  // decoder.  The number of samples in |audio_bus| may vary, and |audio_bus|
  // can be NULL when errors occur.  |is_continuous| is normally true, but will
  // be false if the decoder has detected a frame skip since the last decode
  // operation; and the client should take steps to smooth audio discontinuities
  // in this case.
  using DecodeFrameCallback =
      base::OnceCallback<void(std::unique_ptr<AudioBus> audio_bus,
                              bool is_continuous)>;

  AudioDecoder(const scoped_refptr<CastEnvironment>& cast_environment,
               int channels,
               int sampling_rate,
               Codec codec);

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

  virtual ~AudioDecoder();

  // Returns STATUS_INITIALIZED if the decoder was successfully constructed.  If
  // this method returns any other value, calls to DecodeFrame() will not
  // succeed.
  OperationalStatus InitializationResult() const;

  // Decode the payload in |encoded_frame| asynchronously.  |callback| will be
  // invoked on the CastEnvironment::MAIN thread with the result.
  //
  // In the normal case, |encoded_frame->frame_id| will be
  // monotonically-increasing by 1 for each successive call to this method.
  // When it is not, the decoder will assume one or more frames have been
  // dropped (e.g., due to packet loss), and will perform recovery actions.
  void DecodeFrame(std::unique_ptr<EncodedFrame> encoded_frame,
                   DecodeFrameCallback callback);

 private:
  class ImplBase;
  class OpusImpl;
  class Pcm16Impl;

  const scoped_refptr<CastEnvironment> cast_environment_;
  scoped_refptr<ImplBase> impl_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_TEST_RECEIVER_AUDIO_DECODER_H_
