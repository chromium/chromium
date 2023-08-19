// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_LOGGING_H_
#define MEDIA_AUDIO_AUDIO_LOGGING_H_

#include <memory>
#include <string>


namespace media {

class AudioParameters;

// AudioLog logs state information about an active audio component.
class AudioLog {
 public:
  virtual ~AudioLog() {}

  // Called when an audio component is created.  |params| are the parameters of
  // the created stream.  |device_id| is the id of the audio device opened by
  // the created stream.
  virtual void OnCreated(const media::AudioParameters& params,
                         const std::string& device_id) = 0;

  // Called when an audio component is started, generally this is synonymous
  // with "playing."
  virtual void OnStarted() = 0;

  // Called when an audio component is stopped, generally this is synonymous
  // with "paused."
  virtual void OnStopped() = 0;

  // Called when an audio component is closed, generally this is synonymous
  // with "deleted."
  virtual void OnClosed() = 0;

  // Called when an audio component encounters an error.
  virtual void OnError() = 0;

  // Called when an audio component changes volume.  |volume| is the new volume.
  virtual void OnSetVolume(double volume) = 0;

  // Called with information about audio processing set-up for an audio
  // component.
  virtual void OnProcessingStateChanged(const std::string& message) = 0;

  // Called when an audio component wants to forward a log message.
  virtual void OnLogMessage(const std::string& message) = 0;
};

// AudioLogFactory dispenses AudioLog instances for tracking AudioComponent
// behavior.
class AudioLogFactory {
 public:
  enum class AudioComponent {
    // Input controllers have a 1:1 mapping with streams, so there's no need to
    // track both controllers and streams.
    kAudioInputController,
    // Output controllers may or may not be backed by an active stream, so we
    // need to track both controllers and streams.
    kAudioOuputController,
    kAudioOutputStream,
    kAudiocomponentMax,
  };

  // Create a new AudioLog object for tracking the behavior for one instance of
  // the given component.  Each instance of an "owning" class must create its
  // own AudioLog.
  virtual std::unique_ptr<AudioLog> CreateAudioLog(AudioComponent component,
                                                   int component_id) = 0;

 protected:
  virtual ~AudioLogFactory() {}
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_LOGGING_H_
