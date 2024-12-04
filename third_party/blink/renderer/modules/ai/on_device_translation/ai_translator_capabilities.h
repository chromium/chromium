// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_CAPABILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_CAPABILITIES_H_

#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom-blink-forward.h"
#include "third_party/blink/renderer/modules/ai/ai_capability_availability.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

// This class represents a capabilities object.
class AITranslatorCapabilities final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit AITranslatorCapabilities(
      mojom::blink::TranslatorAvailabilityInfoPtr info);
  ~AITranslatorCapabilities() override = default;

  V8AICapabilityAvailability available() const;
  V8AICapabilityAvailability languagePairAvailable(const String& source,
                                                   const String& target);

  void Trace(Visitor* visitor) const override;

 private:
  const mojom::blink::TranslatorAvailabilityInfoPtr info_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_AI_ON_DEVICE_TRANSLATION_AI_TRANSLATOR_CAPABILITIES_H_
