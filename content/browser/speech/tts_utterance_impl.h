// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SPEECH_TTS_UTTERANCE_IMPL_H_
#define CONTENT_BROWSER_SPEECH_TTS_UTTERANCE_IMPL_H_

#include <set>
#include <string>

#include "base/values.h"
#include "content/public/browser/tts_controller.h"
#include "content/public/browser/tts_utterance.h"

namespace content {
class BrowserContext;

// Implementation of TtsUtterance.
class CONTENT_EXPORT TtsUtteranceImpl : public TtsUtterance {
 public:
  TtsUtteranceImpl(BrowserContext* browser_context);
  ~TtsUtteranceImpl() override;

  // TtsUtterance overrides.
  void OnTtsEvent(TtsEventType event_type,
                  int char_index,
                  int length,
                  const std::string& error_message) override;

  void Finish() override;

  void SetText(const std::string& text) override;
  const std::string& GetText() override;

  void SetOptions(const base::Value* options) override;
  const base::Value* GetOptions() override;

  void SetSrcId(int src_id) override;
  int GetSrcId() override;

  void SetSrcUrl(const GURL& src_url) override;
  const GURL& GetSrcUrl() override;

  void SetVoiceName(const std::string& voice_name) override;
  const std::string& GetVoiceName() override;

  void SetLang(const std::string& lang) override;
  const std::string& GetLang() override;

  void SetContinuousParameters(const double rate,
                               const double pitch,
                               const double volume) override;
  const UtteranceContinuousParameters& GetContinuousParameters() override;

  void SetCanEnqueue(bool can_enqueue) override;
  bool GetCanEnqueue() override;

  void SetRequiredEventTypes(const std::set<TtsEventType>& types) override;
  const std::set<TtsEventType>& GetRequiredEventTypes() override;

  void SetDesiredEventTypes(const std::set<TtsEventType>& types) override;
  const std::set<TtsEventType>& GetDesiredEventTypes() override;

  void SetEngineId(const std::string& engine_id) override;
  const std::string& GetEngineId() override;

  void SetEventDelegate(UtteranceEventDelegate* event_delegate) override;
  UtteranceEventDelegate* GetEventDelegate() override;

  BrowserContext* GetBrowserContext() override;
  int GetId() override;
  bool IsFinished() override;

 private:
  // The BrowserContext that initiated this utterance.
  BrowserContext* browser_context_;

  // The content embedder engine ID of the engine providing TTS for this
  // utterance, or empty if native TTS is being used.
  std::string engine_id_;

  // The unique ID of this utterance, used to associate callback functions
  // with utterances.
  int id_;

  // The id of the next utterance, so we can associate requests with
  // responses.
  static int next_utterance_id_;

  // The text to speak.
  std::string text_;

  // The full options arg passed to tts.speak, which may include fields
  // other than the ones we explicitly parse, below.
  std::unique_ptr<base::Value> options_;

  // The source engine's ID of this utterance, so that it can associate
  // events with the appropriate callback.
  int src_id_;

  // The URL of the page where called speak was called.
  GURL src_url_;

  // The delegate to be called when an utterance event is fired.
  UtteranceEventDelegate* event_delegate_;

  // The parsed options.
  std::string voice_name_;
  std::string lang_;
  UtteranceContinuousParameters continuous_parameters_;
  bool can_enqueue_;
  std::set<TtsEventType> required_event_types_;
  std::set<TtsEventType> desired_event_types_;

  // The index of the current char being spoken.
  int char_index_;

  // True if this utterance received an event indicating it's done.
  bool finished_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SPEECH_TTS_UTTERANCE_IMPL_H_
