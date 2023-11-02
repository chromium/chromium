// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speech/speech_synthesis_base.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"

namespace blink {

SpeechSynthesisBase::SpeechSynthesisBaseCreateFunction
    SpeechSynthesisBase::create_function_ = nullptr;

void SpeechSynthesisBase::Init(SpeechSynthesisBaseCreateFunction function) {
  DCHECK(!create_function_);
  create_function_ = function;
}

SpeechSynthesisBase* SpeechSynthesisBase::Create(LocalDOMWindow& window) {
  DCHECK(create_function_);
  return create_function_(window);
}

void SpeechSynthesisBase::SetOnSpeakingCompletedCallback(
    OnSpeakingCompletedCallback callback) {
  on_speaking_completed_callback_ = std::move(callback);
}

void SpeechSynthesisBase::HandleSpeakingCompleted() {
  if (!on_speaking_completed_callback_.is_null())
    on_speaking_completed_callback_.Run();
}

}  // namespace blink
