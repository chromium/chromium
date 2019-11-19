// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "content/common/content_export.h"
#include "content/public/browser/tts_utterance.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class TtsPlatform;

// Information about one voice.
struct CONTENT_EXPORT VoiceData {
  VoiceData();
  VoiceData(const VoiceData& other);
  ~VoiceData();

  std::string name;
  std::string lang;
  std::string engine_id;
  std::set<TtsEventType> events;

  // If true, the synthesis engine is a remote network resource.
  // It may be higher latency and may incur bandwidth costs.
  bool remote;

  // If true, this is implemented by this platform's subclass of
  // TtsPlatformImpl. If false, this is implemented in a content embedder.
  bool native;
  std::string native_voice_identifier;
};

// Interface that delegates TTS requests to engines in content embedders.
class CONTENT_EXPORT TtsEngineDelegate {
 public:
  virtual ~TtsEngineDelegate() {}

  // Return a list of all available voices registered.
  virtual void GetVoices(BrowserContext* browser_context,
                         std::vector<VoiceData>* out_voices) = 0;

  // Speak the given utterance by sending an event to the given TTS engine.
  virtual void Speak(TtsUtterance* utterance, const VoiceData& voice) = 0;

  // Stop speaking the given utterance by sending an event to the target
  // associated with this utterance.
  virtual void Stop(TtsUtterance* utterance) = 0;

  // Pause in the middle of speaking this utterance.
  virtual void Pause(TtsUtterance* utterance) = 0;

  // Resume speaking this utterance.
  virtual void Resume(TtsUtterance* utterance) = 0;

  // Load the built-in TTS engine.
  virtual bool LoadBuiltInTtsEngine(BrowserContext* browser_context) = 0;
};

// Class that wants to be notified when the set of
// voices has changed.
class CONTENT_EXPORT VoicesChangedDelegate : public base::CheckedObserver {
 public:
  virtual void OnVoicesChanged() = 0;
};

// Singleton class that manages text-to-speech for all TTS engines and
// APIs, maintaining a queue of pending utterances and keeping
// track of all state.
class CONTENT_EXPORT TtsController {
 public:
  // Get the single instance of this class.
  static TtsController* GetInstance();

  // Returns true if we're currently speaking an utterance.
  virtual bool IsSpeaking() = 0;

  // Speak the given utterance. If the utterance's can_enqueue flag is true
  // and another utterance is in progress, adds it to the end of the queue.
  // Otherwise, interrupts any current utterance and speaks this one
  // immediately.
  virtual void SpeakOrEnqueue(std::unique_ptr<TtsUtterance> utterance) = 0;

  // Stop all utterances and flush the queue. Implies leaving pause mode
  // as well.
  virtual void Stop() = 0;

  // Stops the current utterance if it matches the given |source_url|.
  virtual void Stop(const GURL& source_url) = 0;

  // Pause the speech queue. Some engines may support pausing in the middle
  // of an utterance.
  virtual void Pause() = 0;

  // Resume speaking.
  virtual void Resume() = 0;

  // Handle events received from the speech engine. Events are forwarded to
  // the callback function, and in addition, completion and error events
  // trigger finishing the current utterance and starting the next one, if
  // any. If the |char_index| or |length| are not available, the speech engine
  // should pass -1.
  virtual void OnTtsEvent(int utterance_id,
                          TtsEventType event_type,
                          int char_index,
                          int length,
                          const std::string& error_message) = 0;

  // Return a list of all available voices, including the native voice,
  // if supported, and all voices registered by engines.
  virtual void GetVoices(BrowserContext* browser_context,
                         std::vector<VoiceData>* out_voices) = 0;

  // Called by the content embedder or platform implementation when the
  // list of voices may have changed and should be re-queried.
  virtual void VoicesChanged() = 0;

  // Add a delegate that wants to be notified when the set of voices changes.
  virtual void AddVoicesChangedDelegate(VoicesChangedDelegate* delegate) = 0;

  // Remove delegate that wants to be notified when the set of voices changes.
  virtual void RemoveVoicesChangedDelegate(VoicesChangedDelegate* delegate) = 0;

  // Remove delegate that wants to be notified when an utterance fires an event.
  // Note: this cancels speech from any utterance with this delegate, and
  // removes any utterances with this delegate from the queue.
  virtual void RemoveUtteranceEventDelegate(
      UtteranceEventDelegate* delegate) = 0;

  // Set the delegate that processes TTS requests with engines in a content
  // embedder.
  virtual void SetTtsEngineDelegate(TtsEngineDelegate* delegate) = 0;

  // Get the delegate that processes TTS requests with engines in a content
  // embedder.
  virtual TtsEngineDelegate* GetTtsEngineDelegate() = 0;

  // Visible for testing.
  virtual void SetTtsPlatform(TtsPlatform* tts_platform) = 0;
  virtual int QueueSize() = 0;

  virtual void StripSSML(
      const std::string& utterance,
      base::OnceCallback<void(const std::string&)> callback) = 0;

 protected:
  virtual ~TtsController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_CONTROLLER_H_
