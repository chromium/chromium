// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_ai_translator_create_options.h"
#include "third_party/blink/renderer/modules/ai/on_device_translation/ai_translator.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {
class AITranslatorFactory final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AITranslatorFactory();

  ScriptPromise<AITranslator> create(ScriptState* script_state,
                                     AITranslatorCreateOptions* options,
                                     ExceptionState& exception_state);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_FACTORY_H_
