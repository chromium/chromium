// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_MERCHANT_VALIDATION_EVENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_MERCHANT_VALIDATION_EVENT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_merchant_validation_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace WTF {
class AtomicString;
}

namespace blink {

class ScriptState;
class ExceptionState;

class MODULES_EXPORT MerchantValidationEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static MerchantValidationEvent* Create(ScriptState*,
                                         const AtomicString& type,
                                         const MerchantValidationEventInit*,
                                         ExceptionState&);

  MerchantValidationEvent(ScriptState*,
                          const AtomicString& type,
                          const MerchantValidationEventInit*,
                          ExceptionState&);

  MerchantValidationEvent(const MerchantValidationEvent&) = delete;
  MerchantValidationEvent& operator=(const MerchantValidationEvent&) = delete;

  ~MerchantValidationEvent() override;

  const AtomicString& InterfaceName() const override;

  const String& methodName() const;
  const KURL& validationURL() const;
  void complete(ScriptState*, ScriptPromise, ExceptionState&);

 private:
  String method_name_;
  KURL validation_url_;

  // Set to true after .complete() is called.
  bool wait_for_update_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_MERCHANT_VALIDATION_EVENT_H_
