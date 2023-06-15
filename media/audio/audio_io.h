// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_IO_H_
#define MEDIA_AUDIO_AUDIO_IO_H_

#include <stdint.h>

#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/media_export.h"

// Low-level audio output support. To make sound there are 3 objects involved:
// - AudioSource : produces audio samples on a pull model. Implements
//   the AudioSourceCallback interface.
// - AudioOutputStream : uses the AudioSource to render audio on a given
//   channel, format and sample frequency configuration. Data from the
//   AudioSource is delivered in a 'pull' model.
// - AudioManager : factory for the AudioOutputStream objects, manager
//   of the hardware resources and mixer control.
//
// The number and configuration of AudioOutputStream does not need to match the
// physically available hardware resources. For example you can have:
//
//  MonoPCMSource1 --> MonoPCMStream1 --> |       | --> audio left channel
//  StereoPCMSource -> StereoPCMStream -> | mixer |
//  MonoPCMSource2 --> MonoPCMStream2 --> |       | --> audio right channel
//
// This facility's objective is mix and render audio with low overhead using
// the OS basic audio support, abstracting as much as possible the
// idiosyncrasies of each platform. Non-goals:
// - Positional, 3d audio
// - Dependence on non-default libraries such as DirectX 9, 10, XAudio
// - Digital signal processing or effects
// - Extra features if a specific hardware is installed (EAX, X-fi)
//
// The primary client of this facility is audio coming from several tabs.
// Specifically for this case we avoid supporting complex formats such as MP3
// or WMA. Complex format decoding should be done by the renderers.

// Models an audio stream that gets rendered to the audio hardware output.
// Because we support more audio streams than physically available channels
// a given AudioOutputStream might or might not talk directly to hardware.
// An audio stream allocates several buffers for audio data and calls
// AudioSourceCallback::OnMoreData() periodically to fill these buffers,
// as the data is written to the audio device. Size of each packet is determined
// by |samples_per_packet| specified in AudioParameters  when the stream is
// created.

namespace media {

class MEDIA_EXPORT AudioOutputStream {
 public:
  // Audio sources must implement AudioSourceCallback. This interface will be
  // called in a random thread which very likely is a high priority thread. Do
  // not rely on using this thread TLS or make calls that alter the thread
  // itself such as creating Windows or initializing COM.
  class MEDIA_EXPORT AudioSourceCallback {
   public:
    virtual ~AudioSourceCallback() {}

    // Provide more data by fully filling |dest|. The source will return the
    // number of frames it filled. |delay| is the duration of audio written to
    // |dest| in prior calls to OnMoreData() that has not yet been played out,
    // and |delay_timestamp| is the time when |delay| was measured. The time
    // when the first sample added to |dest| is expected to be played out can be
    // calculated by adding |delay| to |delay_timestamp|. The accuracy of
    // |delay| and |delay_timestamp| may vary depending on the platform and
    // implementation. |glitch_info| contains information about all
    // glitches which have occurred since the last call to OnMoreData().
    virtual int OnMoreData(base::TimeDelta delay,
                           base::TimeTicks delay_timestamp,
                           const AudioGlitchInfo& glitch_info,
                           AudioBus* dest) = 0;

    virtual int OnMoreData(base::TimeDelta delay,
                           base::TimeTicks delay_timestamp,
                           const AudioGlitchInfo& glitch_info,
                           AudioBus* dest,
                           bool is_mixing);

    // There was an error while playing a buffer. Audio source cannot be
    // destroyed yet. No direct action needed by the AudioStream, but it is
    // a good place to stop accumulating sound data since is is likely that
    // playback will not continue.
    //
    // An ErrorType may be provided with more information on what went wrong. An
    // unhandled kDeviceChange type error is likely to result in further errors;
    // so it's recommended that sources close their existing output stream and
    // request a new one when this error is sent.
    enum class ErrorType { kUnknown, kDeviceChange };
    virtual void OnError(ErrorType type) = 0;
  };

  virtual ~AudioOutputStream() {}

  // Open the stream. false is returned if the stream cannot be opened.  Open()
  // must always be followed by a call to Close() even if Open() fails.
  virtual bool Open() = 0;

  // Starts playing audio and generating AudioSourceCallback::OnMoreData().
  // Since implementor of AudioOutputStream may have internal buffers, right
  // after calling this method initial buffers are fetched.
  //
  // The output stream does not take ownership of this callback.
  virtual void Start(AudioSourceCallback* callback) = 0;

  // Stops playing audio.  The operation completes synchronously meaning that
  // once Stop() has completed executing, no further callbacks will be made to
  // the callback object that was supplied to Start() and it can be safely
  // deleted. Stop() may be called in any state, e.g. before Start() or after
  // Stop().
  virtual void Stop() = 0;

  // Sets the relative volume, with range [0.0, 1.0] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Gets the relative volume, with range [0.0, 1.0] inclusive.
  virtual void GetVolume(double* volume) = 0;

  // Close the stream.
  // After calling this method, the object should not be used anymore.
  // After calling this method, no further AudioSourceCallback methods
  // should be called on the callback object that was supplied to Start()
  // by the AudioOutputStream implementation.
  virtual void Close() = 0;

  // Flushes the stream. This should only be called if the stream is not
  // playing. (i.e. called after Stop or Open)
  virtual void Flush() = 0;

  // Constrains a timedelta representing a delay to between 0 and 10 seconds.
  // This is used by OS implementations to prevent miscalculated delay values
  // from creating large amounts of noise in the delay stats.
  static base::TimeDelta BoundedDelay(base::TimeDelta delay);
};

// Models an audio sink receiving recorded audio from the audio driver.
class MEDIA_EXPORT AudioInputStream {
 public:
  class MEDIA_EXPORT AudioInputCallback {
   public:
    // Called by the audio recorder when a full packet of audio data is
    // available. This is called from a special audio thread and the
    // implementation should return as soon as possible.
    //
    // |capture_time| is the time at which the first sample in |source| was
    // received. The age of the audio data may be calculated by subtracting
    // |capture_time| from base::TimeTicks::Now(). |capture_time| is always
    // monotonically increasing.
    virtual void OnData(const AudioBus* source,
                        base::TimeTicks capture_time,
                        double volume,
                        const AudioGlitchInfo& audio_glitch_info) = 0;

    // There was an error while recording audio. The audio sink cannot be
    // destroyed yet. No direct action needed by the AudioInputStream, but it
    // is a good place to stop accumulating sound data since is is likely that
    // recording will not continue.
    virtual void OnError() = 0;

   protected:
    virtual ~AudioInputCallback() {}
  };

  virtual ~AudioInputStream() {}

  enum class OpenOutcome {
    kSuccess,
    kAlreadyOpen,
    // Failed due to an unknown or unspecified reason.
    kFailed,
    // Failed to open due to OS-level System permissions.
    kFailedSystemPermissions,
    // Failed to open as the device is exclusively opened by another app.
    kFailedInUse,
  };

  // Open the stream and prepares it for recording. Call Start() to actually
  // begin recording.
  virtual OpenOutcome Open() = 0;

  // Starts recording audio and generating AudioInputCallback::OnData().
  // The input stream does not take ownership of this callback.
  virtual void Start(AudioInputCallback* callback) = 0;

  // Stops recording audio. Effect might not be instantaneous as there could be
  // pending audio callbacks in the queue which will be issued first before
  // recording stops.
  virtual void Stop() = 0;

  // Close the stream. This also generates AudioInputCallback::OnClose(). This
  // should be the last call made on this object.
  virtual void Close() = 0;

  // Returns the maximum microphone analog volume or 0.0 if device does not
  // have volume control.
  virtual double GetMaxVolume() = 0;

  // Sets the microphone analog volume, with range [0, max_volume] inclusive.
  virtual void SetVolume(double volume) = 0;

  // Returns the microphone analog volume, with range [0, max_volume] inclusive.
  virtual double GetVolume() = 0;

  // Sets the Automatic Gain Control (AGC) state.
  virtual bool SetAutomaticGainControl(bool enabled) = 0;

  // Returns the Automatic Gain Control (AGC) state.
  virtual bool GetAutomaticGainControl() = 0;

  // Returns the current muting state for the microphone.
  virtual bool IsMuted() = 0;

  // Sets the output device from which to cancel echo, if echo cancellation is
  // supported by this stream. E.g. called by WebRTC when it changes playback
  // devices.
  virtual void SetOutputDeviceForAec(const std::string& output_device_id) = 0;
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_IO_H_
