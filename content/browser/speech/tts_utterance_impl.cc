// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speech/tts_utterance_impl.h"
#include "base/values.h"
#include "third_party/blink/public/mojom/speech/speech_synthesis.mojom.h"

namespace content {

namespace {

// Returns true if this event type is one that indicates an utterance
// is finished and can be destroyed.
bool IsFinalTtsEventType(TtsEventType event_type) {
  return (event_type == TTS_EVENT_END || event_type == TTS_EVENT_INTERRUPTED ||
          event_type == TTS_EVENT_CANCELLED || event_type == TTS_EVENT_ERROR);
}

}  // namespace

//
// UtteranceContinuousParameters
//

UtteranceContinuousParameters::UtteranceContinuousParameters()
    : rate(blink::mojom::kSpeechSynthesisDoublePrefNotSet),
      pitch(blink::mojom::kSpeechSynthesisDoublePrefNotSet),
      volume(blink::mojom::kSpeechSynthesisDoublePrefNotSet) {}

//
// Utterance
//

// static
int TtsUtteranceImpl::next_utterance_id_ = 0;

std::unique_ptr<TtsUtterance> TtsUtterance::Create(
    BrowserContext* browser_context) {
  return std::make_unique<TtsUtteranceImpl>(browser_context);
}

TtsUtteranceImpl::TtsUtteranceImpl(BrowserContext* browser_context)
    : browser_context_(browser_context),
      id_(next_utterance_id_++),
      src_id_(-1),
      can_enqueue_(false),
      char_index_(0),
      finished_(false) {
  options_.reset(new base::DictionaryValue());
}

TtsUtteranceImpl::~TtsUtteranceImpl() {
  // It's an error if an Utterance is destructed without being finished,
  // unless |browser_context_| is nullptr because it's a unit test.
  DCHECK(finished_ || !browser_context_);
}

void TtsUtteranceImpl::OnTtsEvent(TtsEventType event_type,
                                  int char_index,
                                  int length,
                                  const std::string& error_message) {
  if (char_index >= 0)
    char_index_ = char_index;
  if (IsFinalTtsEventType(event_type))
    finished_ = true;

  if (event_delegate_)
    event_delegate_->OnTtsEvent(this, event_type, char_index, length,
                                error_message);
  if (finished_)
    event_delegate_ = nullptr;
}

void TtsUtteranceImpl::Finish() {
  finished_ = true;
}

void TtsUtteranceImpl::SetText(const std::string& text) {
  text_ = text;
}

const std::string& TtsUtteranceImpl::GetText() {
  return text_;
}

void TtsUtteranceImpl::SetOptions(const base::Value* options) {
  options_.reset(options->DeepCopy());
}

const base::Value* TtsUtteranceImpl::GetOptions() {
  return options_.get();
}

void TtsUtteranceImpl::SetSrcId(int src_id) {
  src_id_ = src_id;
}

int TtsUtteranceImpl::GetSrcId() {
  return src_id_;
}

void TtsUtteranceImpl::SetSrcUrl(const GURL& src_url) {
  src_url_ = src_url;
}
const GURL& TtsUtteranceImpl::GetSrcUrl() {
  return src_url_;
}

void TtsUtteranceImpl::SetVoiceName(const std::string& voice_name) {
  voice_name_ = voice_name;
}

const std::string& TtsUtteranceImpl::GetVoiceName() {
  return voice_name_;
}

void TtsUtteranceImpl::SetLang(const std::string& lang) {
  lang_ = lang;
}

const std::string& TtsUtteranceImpl::GetLang() {
  return lang_;
}

void TtsUtteranceImpl::SetContinuousParameters(const double rate,
                                               const double pitch,
                                               const double volume) {
  continuous_parameters_.rate = rate;
  continuous_parameters_.pitch = pitch;
  continuous_parameters_.volume = volume;
}

const UtteranceContinuousParameters&
TtsUtteranceImpl::GetContinuousParameters() {
  return continuous_parameters_;
}

void TtsUtteranceImpl::SetCanEnqueue(bool can_enqueue) {
  can_enqueue_ = can_enqueue;
}

bool TtsUtteranceImpl::GetCanEnqueue() {
  return can_enqueue_;
}

void TtsUtteranceImpl::SetRequiredEventTypes(
    const std::set<TtsEventType>& types) {
  required_event_types_ = types;
}

const std::set<TtsEventType>& TtsUtteranceImpl::GetRequiredEventTypes() {
  return required_event_types_;
}

void TtsUtteranceImpl::SetDesiredEventTypes(
    const std::set<TtsEventType>& types) {
  desired_event_types_ = types;
}
const std::set<TtsEventType>& TtsUtteranceImpl::GetDesiredEventTypes() {
  return desired_event_types_;
}

void TtsUtteranceImpl::SetEngineId(const std::string& engine_id) {
  engine_id_ = engine_id;
}

const std::string& TtsUtteranceImpl::GetEngineId() {
  return engine_id_;
}

void TtsUtteranceImpl::SetEventDelegate(
    UtteranceEventDelegate* event_delegate) {
  event_delegate_ = event_delegate;
}

UtteranceEventDelegate* TtsUtteranceImpl::GetEventDelegate() {
  return event_delegate_;
}

BrowserContext* TtsUtteranceImpl::GetBrowserContext() {
  return browser_context_;
}

int TtsUtteranceImpl::GetId() {
  return id_;
}

bool TtsUtteranceImpl::IsFinished() {
  return finished_;
}

}  // namespace content
