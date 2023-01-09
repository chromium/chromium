// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_ERROR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_ERROR_H_

#include "third_party/blink/public/mojom/smart_card/smart_card.mojom-shared.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_response_code.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class SmartCardErrorOptions;

// https://w3c.github.io/webtransport/#web-transport-error-interface
class MODULES_EXPORT SmartCardError : public DOMException {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Constructor exposed to script. Called by the V8 bindings.
  static SmartCardError* Create(String message, const SmartCardErrorOptions*);

  explicit SmartCardError(mojom::SmartCardResponseCode mojom_response_code);
  SmartCardError(String message, V8SmartCardResponseCode);
  ~SmartCardError() override;

  V8SmartCardResponseCode responseCode() const { return response_code_; }

 private:
  const V8SmartCardResponseCode response_code_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_ERROR_H_
