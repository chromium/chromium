// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_
#define CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"
#include "url/gurl.h"

namespace content {

// Interface for TTS functionality provided by the platform (e.g. on ChromeOS
// by Ash while being used for LaCrOS).
class CONTENT_EXPORT ExternalPlatformDelegate {
 public:
  virtual ~ExternalPlatformDelegate() = default;

  // Returns a list of all available voices for |browser_context|, including
  // the ones registered with the external TtsController living in another
  // browser process. The voices include both the native voice, if supported,
  // and all voices registered by engines.
  // |source_url| will be used for policy decisions by engines to determine
  // which voices to return.
  virtual void GetVoicesForBrowserContext(
      BrowserContext* browser_context,
      const GURL& source_url,
      std::vector<VoiceData>* out_voices) = 0;

  // Enqueues the given utterance to the external TtsController. The
  // utterance will be added to the utterance queue of the external
  // TtsController and processed in sequence.
  virtual void Enqueue(std::unique_ptr<TtsUtterance> utterance) = 0;

  // Requests external TtsController to stop the current utterance if it matches
  // the given |source_url|.
  virtual void Stop(const GURL& source_url) = 0;

  // Requests external TtsController to pause speech synthesis.
  virtual void Pause() = 0;

  // Requests external TtsController to resume speech synthesis.
  virtual void Resume() = 0;
};

// Abstract class that defines the native platform TTS interface,
// subclassed by specific implementations on Win, Mac, etc.
class CONTENT_EXPORT TtsPlatform {
 public:
  static TtsPlatform* GetInstance();

  // Returns true if this platform implementation is supported. The returned
  // value of this method won't change over time.
  virtual bool PlatformImplSupported() = 0;

  // Returns true if this platform implementation is initialized. If the
  // platform is supported, this method will eventually return true, when
  // the asynchronous initialisation is completed. Other methods may fail if
  // called when not yet initialized.
  virtual bool PlatformImplInitialized() = 0;

  // Some platforms may provide a built-in TTS engine. Returns true
  // if the engine was not previously loaded and is now loading, and
  // false if it's already loaded or if there's no engine to load.
  // Will call TtsController::RetrySpeakingQueuedUtterances when
  // the engine finishes loading.
  virtual void LoadBuiltInTtsEngine(BrowserContext* browser_context) = 0;

  // Speak the given utterance using the native voice provided by the platform
  // with the given parameters if possible.
  // Utterance will always be nonempty. If rate, pitch, or volume are -1.0,
  // they will be ignored.
  //
  // The TtsController will only try to speak one utterance at
  // a time. If it wants to interrupt speech, it will always call Stop
  // before speaking again.
  //
  // |did_start_speaking_callback| is called (either sync or async) when either
  // speech was started (value of true), or if speech isn't possible at the
  // current time (value of false).
  virtual void Speak(
      int utterance_id,
      const std::string& utterance,
      const std::string& lang,
      const VoiceData& voice,
      const UtteranceContinuousParameters& params,
      base::OnceCallback<void(bool)> did_start_speaking_callback) = 0;

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

  // If supported, the platform shutdown its internal state. After that call,
  // other methods may no-op.
  virtual void Shutdown() = 0;

  // Given engine delegate and platform voices, returns the finalized voice
  // ordering used by the controller when exposing voices to clients.
  virtual void FinalizeVoiceOrdering(std::vector<VoiceData>& voices) = 0;

  // Triggers the TtsPlatform to update its list of voices and relay that update
  // through VoicesChanged.
  virtual void RefreshVoices() = 0;

  // Gets the delegate that routes TTS requests to the external TtsController.
  virtual ExternalPlatformDelegate* GetExternalPlatformDelegate() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_PLATFORM_H_
