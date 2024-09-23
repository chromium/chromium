// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_EVENT_LISTENER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_EVENT_LISTENER_H_

#include "content/common/content_export.h"
#include "media/mojo/mojom/speech_recognition_result.mojom.h"

namespace media::mojom {
class SpeechRecognitionError;
}  // namespace media::mojom

namespace content {

// The interface to be implemented by consumers interested in receiving
// speech recognition events.
class CONTENT_EXPORT SpeechRecognitionEventListener {
 public:
  // Invoked when the StartRequest is received and the recognition process is
  // started.
  virtual void OnRecognitionStart(int session_id) = 0;

  // Invoked when the first audio capture is initiated.
  virtual void OnAudioStart(int session_id) = 0;

  // Informs that the endpointer has started detecting sound (possibly speech).
  virtual void OnSoundStart(int session_id) = 0;

  // Informs that the endpointer has stopped detecting sound (a long silence).
  virtual void OnSoundEnd(int session_id) = 0;

  // Invoked when audio capture stops, either due to the endpoint detecting
  // silence, an internal error, or an explicit stop was issued.
  virtual void OnAudioEnd(int session_id) = 0;

  // Invoked when a result is retrieved.
  virtual void OnRecognitionResults(
      int session_id,
      const std::vector<media::mojom::WebSpeechRecognitionResultPtr>&
          results) = 0;

  // Invoked if there was an error while capturing or recognizing audio.
  // The recognition has already been cancelled when this call is made and
  // no more events will be raised.
  virtual void OnRecognitionError(
      int session_id,
      const media::mojom::SpeechRecognitionError& error) = 0;

  // Informs of a change in the captured audio level, useful if displaying
  // a microphone volume indicator while recording.
  // The value of |volume| and |noise_volume| is in the [0.0, 1.0] range.
  // TODO(janx): Is this necessary? It looks like only x-webkit-speech bubble
  // uses it (see crbug.com/247351).
  virtual void OnAudioLevelsChange(int session_id,
                                   float volume, float noise_volume) = 0;

  // This is guaranteed to be the last event raised in the recognition
  // process and the |SpeechRecognizer| object can be freed if necessary.
  virtual void OnRecognitionEnd(int session_id) = 0;

 protected:
  virtual ~SpeechRecognitionEventListener() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_EVENT_LISTENER_H_
