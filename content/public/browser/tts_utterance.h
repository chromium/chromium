// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TTS_UTTERANCE_H_
#define CONTENT_PUBLIC_BROWSER_TTS_UTTERANCE_H_

#include <memory>
#include <set>

#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class TtsUtterance;
class WebContents;

// Events sent back from the TTS engine indicating the progress.
enum TtsEventType {
  TTS_EVENT_START,
  TTS_EVENT_END,
  TTS_EVENT_WORD,
  TTS_EVENT_SENTENCE,
  TTS_EVENT_MARKER,
  TTS_EVENT_INTERRUPTED,
  TTS_EVENT_CANCELLED,
  TTS_EVENT_ERROR,
  TTS_EVENT_PAUSE,
  TTS_EVENT_RESUME
};

// The continuous parameters that apply to a given utterance.
struct CONTENT_EXPORT UtteranceContinuousParameters {
  UtteranceContinuousParameters();

  double rate;
  double pitch;
  double volume;
};

// Returns true if this event type is one that indicates an utterance
// is finished and can be destroyed.
CONTENT_EXPORT bool IsFinalTtsEventType(TtsEventType event_type);

// Class that wants to receive events on utterances.
class CONTENT_EXPORT UtteranceEventDelegate {
 public:
  virtual ~UtteranceEventDelegate() {}
  // Called when the engine reaches a TTS event in an utterance. If |char_index|
  // or |length| are invalid or not applicable for the given |event_type|, they
  // should be set to -1.
  virtual void OnTtsEvent(TtsUtterance* utterance,
                          TtsEventType event_type,
                          int char_index,
                          int length,
                          const std::string& error_message) = 0;
};

// One speech utterance.
class CONTENT_EXPORT TtsUtterance {
 public:
  // Construct an utterance given a WebContents, BrowserContext, or no backing
  // context. Prefer the more specific WebContents variant if possible. The
  // utterance's speaking lifetime is tied to their lifetime.
  // Before speaking this utterance, its other parameters like text, rate,
  // pitch, etc. should all be set.
  static std::unique_ptr<TtsUtterance> Create(WebContents* web_contents);
  // |should_always_be_spoken|: See comment for ShouldAlwaysBeSpoken().
  static std::unique_ptr<TtsUtterance> Create(
      BrowserContext* browser_context,
      bool should_always_be_spoken = false);
  static std::unique_ptr<TtsUtterance> Create();

  virtual ~TtsUtterance() = default;

  // Sends an event to the delegate. If the event type is TTS_EVENT_END
  // or TTS_EVENT_ERROR, deletes the utterance. If |char_index| is -1,
  // uses the last good value. If |length| is -1, that represents an unknown
  // length, and will simply be passed to the delegate as -1.
  virtual void OnTtsEvent(TtsEventType event_type,
                          int char_index,
                          int length,
                          const std::string& error_message) = 0;

  // Finish an utterance without sending an event to the delegate.
  virtual void Finish() = 0;

  // Getters and setters for the text to speak and other speech options.
  virtual void SetText(const std::string& text) = 0;
  virtual const std::string& GetText() = 0;

  virtual void SetOptions(base::Value::Dict options) = 0;
  virtual const base::Value::Dict* GetOptions() = 0;

  virtual void SetSrcId(int src_id) = 0;
  virtual int GetSrcId() = 0;

  virtual void SetSrcUrl(const GURL& src_url) = 0;
  virtual const GURL& GetSrcUrl() = 0;

  virtual void SetVoiceName(const std::string& voice_name) = 0;
  virtual const std::string& GetVoiceName() = 0;

  virtual void SetLang(const std::string& lang) = 0;
  virtual const std::string& GetLang() = 0;

  virtual void SetContinuousParameters(const double rate,
                                       const double pitch,
                                       const double volume) = 0;
  virtual const UtteranceContinuousParameters& GetContinuousParameters() = 0;

  // Prior to processing this utterance, determines whether the utterance queue
  // gets cleared.
  virtual void SetShouldClearQueue(bool value) = 0;
  virtual bool GetShouldClearQueue() = 0;

  virtual void SetRequiredEventTypes(const std::set<TtsEventType>& types) = 0;
  virtual const std::set<TtsEventType>& GetRequiredEventTypes() = 0;

  virtual void SetDesiredEventTypes(const std::set<TtsEventType>& types) = 0;
  virtual const std::set<TtsEventType>& GetDesiredEventTypes() = 0;

  virtual void SetEngineId(const std::string& engine_id) = 0;
  virtual const std::string& GetEngineId() = 0;

  virtual void SetEventDelegate(UtteranceEventDelegate* event_delegate) = 0;
  virtual UtteranceEventDelegate* GetEventDelegate() = 0;

  // Getters and setters for internal state.
  virtual BrowserContext* GetBrowserContext() = 0;
  virtual void ClearBrowserContext() = 0;
  virtual int GetId() = 0;
  virtual bool IsFinished() = 0;
  virtual WebContents* GetWebContents() = 0;

  // An utterance could become invalid (for example, its associated WebContents
  // has been destroyed) and therefore should not be spoken when it is
  // processed by TtsController from the utterance queue. If this function
  // returns true, it guarantees that the utterance must be a valid one and
  // should be spoken.
  virtual bool ShouldAlwaysBeSpoken() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TTS_UTTERANCE_H_
