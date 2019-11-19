// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_

#include "base/callback.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/speech/speech_recognition_result.mojom.h"

namespace content {

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

 protected:
  virtual ~SpeechRecognitionManager() {}

 private:
  static SpeechRecognitionManager* manager_for_tests_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_H_
