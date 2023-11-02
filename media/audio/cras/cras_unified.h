// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates a unified stream based on the cras (ChromeOS audio server) interface.
//
// CrasUnifiedStream object is *not* thread-safe and should only be used
// from the audio thread.

#ifndef MEDIA_AUDIO_CRAS_CRAS_UNIFIED_H_
#define MEDIA_AUDIO_CRAS_CRAS_UNIFIED_H_

#include <cras_client.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioManagerCrasBase;

// Implementation of AudioOuputStream for Chrome OS using the Chrome OS audio
// server.
// TODO(dgreid): This class is used for only output, either remove all the
// relevant input code and change the class to CrasOutputStream or merge
// cras_input.cc into this unified implementation.
class MEDIA_EXPORT CrasUnifiedStream : public AudioOutputStream {
 public:
  // The ctor takes all the usual parameters, plus |manager| which is the
  // audio manager who is creating this object.
  CrasUnifiedStream(const AudioParameters& params,
                    AudioManagerCrasBase* manager,
                    const std::string& device_id);

  CrasUnifiedStream(const CrasUnifiedStream&) = delete;
  CrasUnifiedStream& operator=(const CrasUnifiedStream&) = delete;

  // The dtor is typically called by the AudioManager only and it is usually
  // triggered by calling AudioUnifiedStream::Close().
  ~CrasUnifiedStream() override;

  // Implementation of AudioOutputStream.
  bool Open() override;
  void Close() override;
  void Flush() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  // Handles captured audio and fills the output with audio to be played.
  static int UnifiedCallback(struct libcras_stream_cb_data* data);

  // Handles notification that there was an error with the playback stream.
  static int StreamError(cras_client* client,
                         cras_stream_id_t stream_id,
                         int err,
                         void* arg);

  // Writes audio for a playback stream.
  uint32_t WriteAudio(size_t frames,
                      uint8_t* buffer,
                      const timespec* latency_ts);

  // Deals with an error that occured in the stream.  Called from StreamError().
  void NotifyStreamError(int err);

  // The client used to communicate with the audio server.
  struct libcras_client* client_;

  // ID of the playing stream.
  cras_stream_id_t stream_id_;

  // PCM parameters for the stream.
  AudioParameters params_;

  // True if stream is playing.
  bool is_playing_;

  // Volume level from 0.0 to 1.0.
  float volume_;

  // Audio manager that created us.  Used to report that we've been closed.
  AudioManagerCrasBase* manager_;

  // Callback to get audio samples.
  AudioSourceCallback* source_callback_;

  // Container for exchanging data with AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> output_bus_;

  // Direciton of the stream.
  CRAS_STREAM_DIRECTION stream_direction_;

  // Index of the CRAS device to stream output to.
  const int pin_device_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_CRAS_CRAS_UNIFIED_H_
