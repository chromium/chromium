// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SPEECH_SPEECH_SYNTHESIS_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SPEECH_SPEECH_SYNTHESIS_BASE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// SpeechSynthesisBase is the parent class of SpeechSynthesis in /modules. The
// primary function of this class is to make SpeechSynthesis methods accessible
// in /core. /core is where all audio description handling occurs.
class CORE_EXPORT SpeechSynthesisBase : public GarbageCollectedMixin {
 public:
  SpeechSynthesisBase(const SpeechSynthesisBase&) = delete;
  SpeechSynthesisBase& operator=(const SpeechSynthesisBase&) = delete;
  ~SpeechSynthesisBase() = default;

  typedef SpeechSynthesisBase* (*SpeechSynthesisBaseCreateFunction)(
      LocalDOMWindow&);

  // GarbageCollectedMixin
  void Trace(Visitor* visitor) const override {}

  // Sets create_function_.
  // Init is intended to be called in modules_initializer.cc, where
  // SpeechSynthesisBase has access to a create function defined in
  // /modules/../speech_synthesis.cc.
  static void Init(SpeechSynthesisBaseCreateFunction function);

  // Uses create_function_ set in Init to instantiate a new SpeechSynthesisBase
  // object. create_function_ must take in a LocalDOMWindow, so Create also
  // takes in LocalDOMWindow.
  // Create is intended to be called in /core.
  static SpeechSynthesisBase* Create(LocalDOMWindow& window);

  // Overridden in speech_synthesis.cc.
  virtual void Speak(const String& text) = 0;
  virtual void Cancel() = 0;

 protected:
  SpeechSynthesisBase() = default;

 private:
  // Creates a SpeechSynthesis object returned as type SpeechSynthesisBase for
  // use in /core.
  static SpeechSynthesisBaseCreateFunction create_function_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SPEECH_SPEECH_SYNTHESIS_BASE_H_
