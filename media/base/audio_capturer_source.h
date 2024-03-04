// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
#define MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"

namespace media {

class AudioProcessorControls;
struct AudioGlitchInfo;

// AudioCapturerSource is an interface representing the source for
// captured audio. An implementation will periodically call
// `CaptureCallback::Capture()` to pass the audio samples. All methods must be
// called on the same thread. The supplied `CaptureCallback` will be called on
// the same thread, except `Capture()`. Implementations should use a dedicated
// thread to receive the stream and call `CaptureCallback::Capture()` in order
// to ensure that the client receives the stream without delays.
class AudioCapturerSource
    : public base::RefCountedThreadSafe<media::AudioCapturerSource> {
 public:
  enum class ErrorCode {
    kUnknown = 0,
    kSystemPermissions = 1,
    kDeviceInUse = 2,
    kSocketError = 3,
  };

  class CaptureCallback {
   public:
    // Signals that audio recording has been started.  Called asynchronously
    // after Start() has completed. If Start() encounters problems before this
    // callback can be made, OnCaptureError will be called instead.
    // This callback is provided for sources such as local audio sources that
    // require asynchronous initialization so not all sources will support this
    // notification.
    virtual void OnCaptureStarted() {}

    // Callback to deliver the captured data from the stream. Called from a
    // thread that's different from the thread used for all other methods.
    virtual void Capture(const AudioBus* audio_source,
                         base::TimeTicks audio_capture_time,
                         const AudioGlitchInfo& glitch_info,
                         double volume,
                         bool key_pressed) = 0;

    // Signals an error has occurred.
    virtual void OnCaptureError(ErrorCode code, const std::string& message) = 0;

    // Signals the muted state has changed. May be called before
    // OnCaptureStarted.
    virtual void OnCaptureMuted(bool is_muted) = 0;

    // For streams created with audio processing, signals that a controllable
    // audio processor has been created.
    virtual void OnCaptureProcessorCreated(AudioProcessorControls* controls) {}

   protected:
    virtual ~CaptureCallback() {}
  };

  // Sets information about the audio stream format and the device to be used.
  // It must be called exactly once before any of the other methods.
  virtual void Initialize(const AudioParameters& params,
                          CaptureCallback* callback) = 0;

  // Starts the audio recording.
  virtual void Start() = 0;

  // Stops the audio recording. This API is synchronous, and no more data
  // callback will be passed to the client after it is being called.
  virtual void Stop() = 0;

  // Sets the capture volume, with range [0.0, 1.0] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Enables or disables the WebRtc AGC control.
  virtual void SetAutomaticGainControl(bool enable) = 0;

  // Sets the output device the source should cancel echo from, if
  // supported. Must be called on the main thread. Device ids are gotten from
  // device enumerations.
  virtual void SetOutputDeviceForAec(const std::string& output_device_id) = 0;

 protected:
  friend class base::RefCountedThreadSafe<AudioCapturerSource>;
  virtual ~AudioCapturerSource() {}
};

}  // namespace media

#endif  // MEDIA_BASE_AUDIO_CAPTURER_SOURCE_H_
