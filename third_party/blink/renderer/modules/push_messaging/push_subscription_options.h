// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_OPTIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_OPTIONS_H_

#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMArrayBuffer;
class ExceptionState;
class PushSubscriptionOptionsInit;

class PushSubscriptionOptions final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  // Converts developer-provided dictionary to PushSubscriptionOptions.
  // Throws if applicationServerKey is invalid.
  static MODULES_EXPORT PushSubscriptionOptions* FromOptionsInit(
      const PushSubscriptionOptionsInit* options_init,
      ExceptionState& exception_state);

  static PushSubscriptionOptions* Create(
      bool user_visible_only,
      const WTF::Vector<uint8_t>& application_server_key) {
    return MakeGarbageCollected<PushSubscriptionOptions>(
        user_visible_only, application_server_key);
  }

  explicit PushSubscriptionOptions(
      bool user_visible_only,
      const WTF::Vector<uint8_t>& application_server_key);

  bool userVisibleOnly() const { return user_visible_only_; }

  // Mutable by web developer. See https://github.com/w3c/push-api/issues/198.
  DOMArrayBuffer* applicationServerKey() const {
    return application_server_key_;
  }

  // Whether the application server key follows the VAPID protocol.
  bool IsApplicationServerKeyVapid() const;

  void Trace(blink::Visitor* visitor) override;

 private:
  bool user_visible_only_;
  Member<DOMArrayBuffer> application_server_key_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_SUBSCRIPTION_OPTIONS_H_
