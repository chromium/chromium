// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_AUDIO_RENDERER_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_AUDIO_RENDERER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "media/base/output_device_info.h"

namespace blink {

class WebMediaStreamAudioRenderer
    : public base::RefCountedThreadSafe<WebMediaStreamAudioRenderer> {
 public:
  // Starts rendering audio.
  virtual void Start() = 0;

  // Stops rendering audio.
  virtual void Stop() = 0;

  // Resumes rendering audio after being paused.
  virtual void Play() = 0;

  // Temporarily suspends rendering audio. The audio stream might still be
  // active but new audio data is not provided to the consumer.
  virtual void Pause() = 0;

  // Sets the output volume.
  virtual void SetVolume(float volume) = 0;

  // Attempts to switch the audio output device.
  // Once the attempt is finished, |callback| is invoked with the result of the
  // operation passed as a parameter. The result is a value from the
  // media::OutputDeviceStatus enum.
  // There is no guarantee about the thread where |callback| will be invoked.
  // TODO(olka): make sure callback is always called on the client thread,
  // update clients accordingly and fix the comment.
  virtual void SwitchOutputDevice(const std::string& device_id,
                                  media::OutputDeviceStatusCB callback) = 0;

  // Time stamp that reflects the current render time. Should not be updated
  // when paused.
  virtual base::TimeDelta GetCurrentRenderTime() = 0;

 protected:
  friend class base::RefCountedThreadSafe<WebMediaStreamAudioRenderer>;

  virtual ~WebMediaStreamAudioRenderer() {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_AUDIO_RENDERER_H_
