// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_
#define CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_

#include <string>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"

namespace content {

// Abstract class that defines the native platform TTS interface,
// subclassed by specific implementations on Win, Mac, etc.
class CONTENT_EXPORT TtsPlatform {
 public:
  static TtsPlatform* GetInstance();

  // Returns true if this platform implementation is supported and available.
  virtual bool PlatformImplAvailable() = 0;

  // Some platforms may provide a built-in TTS engine. Returns true
  // if the engine was not previously loaded and is now loading, and
  // false if it's already loaded or if there's no engine to load.
  // Will call TtsController::RetrySpeakingQueuedUtterances when
  // the engine finishes loading.
  virtual bool LoadBuiltInTtsEngine(BrowserContext* browser_context) = 0;

  // Speak the given utterance with the given parameters if possible,
  // and return true on success. Utterance will always be nonempty.
  // If rate, pitch, or volume are -1.0, they will be ignored.
  //
  // The TtsController will only try to speak one utterance at
  // a time. If it wants to interrupt speech, it will always call Stop
  // before speaking again.
  virtual void Speak(int utterance_id,
                     const std::string& utterance,
                     const std::string& lang,
                     const VoiceData& voice,
                     const UtteranceContinuousParameters& params,
                     base::OnceCallback<void(bool)> on_speak_finished) = 0;

  // Stop speaking immediately and return true on success.
  virtual bool StopSpeaking() = 0;

  // Returns whether any speech is on going.
  virtual bool IsSpeaking() = 0;

  // Append information about voices provided by this platform implementation
  // to |out_voices|.
  virtual void GetVoices(std::vector<VoiceData>* out_voices) = 0;

  // Pause the current utterance, if any, until a call to Resume,
  // Speak, or StopSpeaking.
  virtual void Pause() = 0;

  // Resume speaking the current utterance, if it was paused.
  virtual void Resume() = 0;

  // Allows the platform to monitor speech commands and the voices used
  // for each one.
  virtual void WillSpeakUtteranceWithVoice(TtsUtterance* utterance,
                                           const VoiceData& voice_data) = 0;

  virtual std::string GetError() = 0;
  virtual void ClearError() = 0;
  virtual void SetError(const std::string& error) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_
