// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AudioOutputDispatcher is a single-threaded base class that dispatches
// creation and deletion of audio output streams. AudioOutputProxy objects use
// this class to allocate and recycle actual audio output streams. When playback
// is started, the proxy calls StartStream() to get an output stream that it
// uses to play audio. When playback is stopped, the proxy returns the stream
// back to the dispatcher by calling StopStream().
//
// AudioManagerBase creates one specialization of AudioOutputDispatcher on the
// audio thread for each possible set of audio parameters. I.e streams with
// different parameters are managed independently.  The AudioOutputDispatcher
// instance is then deleted on the audio thread when the AudioManager shuts
// down.

#ifndef MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_
#define MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_

#include "base/memory/raw_ptr.h"
#include "media/audio/audio_io.h"

namespace media {
class AudioManager;
class AudioOutputProxy;

// Lives and must be called on AudioManager device thread.
class MEDIA_EXPORT AudioOutputDispatcher {
 public:
  AudioOutputDispatcher(AudioManager* audio_manager);

  AudioOutputDispatcher(const AudioOutputDispatcher&) = delete;
  AudioOutputDispatcher& operator=(const AudioOutputDispatcher&) = delete;

  virtual ~AudioOutputDispatcher();

  // Creates an instance of AudioOutputProxy, which uses |this| as dispatcher.
  // The client owns the returned pointer, which can be deleted using
  // AudioOutputProxy::Close.
  virtual AudioOutputProxy* CreateStreamProxy() = 0;

  // Called by AudioOutputProxy to open the stream.
  // Returns false, if it fails to open it.
  virtual bool OpenStream() = 0;

  // Called by AudioOutputProxy when the stream is started.
  // Uses |callback| to get source data and report errors, if any.
  // Does *not* take ownership of this callback.
  // Returns true if started successfully, false otherwise.
  virtual bool StartStream(AudioOutputStream::AudioSourceCallback* callback,
                           AudioOutputProxy* stream_proxy) = 0;

  // Called by AudioOutputProxy when the stream is stopped.
  // Ownership of the |stream_proxy| is passed to the dispatcher.
  virtual void StopStream(AudioOutputProxy* stream_proxy) = 0;

  // Called by AudioOutputProxy when the volume is set.
  virtual void StreamVolumeSet(AudioOutputProxy* stream_proxy,
                               double volume) = 0;

  // Called by AudioOutputProxy when the stream is closed.
  virtual void CloseStream(AudioOutputProxy* stream_proxy) = 0;

  // Called by AudioOutputProxy to flush the stream.  This should only be
  // called when a stream is stopped.
  virtual void FlushStream(AudioOutputProxy* stream_proxy) = 0;

 protected:
  AudioManager* audio_manager() const { return audio_manager_; }

 private:
  // A no-reference-held pointer (we don't want circular references) back to the
  // AudioManager that owns this object.
  const raw_ptr<AudioManager> audio_manager_;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_OUTPUT_DISPATCHER_H_
