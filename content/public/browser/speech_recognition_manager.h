// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"
#include "media/mojo/mojom/speech_recognition_audio_forwarder.mojom.h"
#include "media/mojo/mojom/speech_recognizer.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

struct SpeechRecognitionAudioForwarderConfig;
class SpeechRecognitionEventListener;
struct SpeechRecognitionSessionConfig;
struct SpeechRecognitionSessionContext;

// The SpeechRecognitionManager (SRM) is a singleton class that handles SR
// functionalities within Chrome. Everyone that needs to perform SR should
// interface exclusively with the SRM, receiving events through the callback
// interface SpeechRecognitionEventListener.
// Since many different sources can use SR in different times (some overlapping
// is allowed while waiting for results), the SRM has the further responsibility
// of handling separately and reliably (taking into account also call sequences
// that might not make sense, e.g., two subsequent AbortSession calls).
// In this sense a session, within the SRM, models the ongoing evolution of a
// SR request from the viewpoint of the end-user, abstracting all the concrete
// operations that must be carried out, that will be handled by inner classes.
class SpeechRecognitionManager {
 public:
  enum { kSessionIDInvalid = 0 };

  // Returns the singleton instance.
  static CONTENT_EXPORT SpeechRecognitionManager* GetInstance();

  // Singleton manager setter useful for tests.
  static void CONTENT_EXPORT SetManagerForTesting(
      SpeechRecognitionManager* manager);

  // Creates a new recognition session.
  virtual int CreateSession(const SpeechRecognitionSessionConfig& config) = 0;

  // Creates a new recognition session. If the session mojo remotes are not
  // null, speech recognition session will be managed by the speech recognition
  // service, otherwise the session will be managed by the browser. If the audio
  // forwarder config is not null, the audio forwarder will be used to receive
  // audio, otherwise the audio will be received from the microphone.
  virtual int CreateSession(
      const SpeechRecognitionSessionConfig& config,
      mojo::PendingReceiver<media::mojom::SpeechRecognitionSession>
          session_receiver,
      mojo::PendingRemote<media::mojom::SpeechRecognitionSessionClient>
          client_remote,
      std::optional<SpeechRecognitionAudioForwarderConfig>
          audio_forwarder_config) = 0;

  // Starts/restarts recognition for an existing session, after performing a
  // preliminary check on the delegate (CheckRecognitionIsAllowed).
  virtual void StartSession(int session_id) = 0;

  // Aborts recognition for an existing session, without providing any result.
  virtual void AbortSession(int session_id) = 0;

  // Aborts all sessions for a given RenderFrame, without providing any result.
  virtual void AbortAllSessionsForRenderFrame(int render_process_id,
                                              int render_frame_id) = 0;

  // Stops audio capture for an existing session. The audio captured before the
  // call will be processed, possibly ending up with a result.
  virtual void StopAudioCaptureForSession(int session_id) = 0;

  // Retrieves the configuration of a session, as provided by the caller
  // upon CreateSession.
  virtual const SpeechRecognitionSessionConfig& GetSessionConfig(
      int session_id) = 0;

  // Retrieves the context associated to a session.
  virtual SpeechRecognitionSessionContext GetSessionContext(int session_id) = 0;

  virtual bool UseOnDeviceSpeechRecognition(
      const SpeechRecognitionSessionConfig& config) = 0;

 protected:
  virtual ~SpeechRecognitionManager() {}

 private:
  static SpeechRecognitionManager* manager_for_tests_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
