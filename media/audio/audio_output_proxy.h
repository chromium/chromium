// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

namespace media {

class AudioOutputDispatcher;

// AudioOutputProxy is an audio output stream that uses resources more
// efficiently than a regular audio output stream: it opens audio
// device only when sound is playing, i.e. between Start() and Stop()
// (there is still one physical stream per each audio output proxy in
// playing state).
//
// AudioOutputProxy uses AudioOutputDispatcher to open and close
// physical output streams.
class MEDIA_EXPORT AudioOutputProxy : public AudioOutputStream {
 public:
  // Caller keeps ownership of |dispatcher|.
  explicit AudioOutputProxy(base::WeakPtr<AudioOutputDispatcher> dispatcher);

  AudioOutputProxy(const AudioOutputProxy&) = delete;
  AudioOutputProxy& operator=(const AudioOutputProxy&) = delete;

  // AudioOutputStream interface.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;
  void Flush() override;

  AudioOutputDispatcher* get_dispatcher_for_testing() const {
    return dispatcher_.get();
  }

 private:
  enum State {
    kCreated,
    kOpened,
    kPlaying,
    kClosed,
    kOpenError,
    kStartError,
  };

  ~AudioOutputProxy() override;

  base::WeakPtr<AudioOutputDispatcher> dispatcher_;
  State state_;

  // Need to save volume here, so that we can restore it in case the stream
  // is stopped, and then started again.
  double volume_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_PROXY_H_
