// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class SMSReceiverOptions;
class ScriptPromiseResolver;

class SMSReceiver final : public ScriptWrappable, public ContextClient {
  USING_GARBAGE_COLLECTED_MIXIN(SMSReceiver);
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SMSReceiver(ExecutionContext*);

  ~SMSReceiver() override;

  // SMSReceiver IDL interface.
  ScriptPromise receive(ScriptState*, const SMSReceiverOptions*);

  void Trace(blink::Visitor*) override;

 private:
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;

  void OnReceive(ScriptPromiseResolver* resolver,
                 base::TimeTicks start_time,
                 mojom::blink::SmsStatus status,
                 const WTF::String& sms);

  void OnSMSReceiverConnectionError();

  mojo::Remote<mojom::blink::SmsReceiver> service_;

  DISALLOW_COPY_AND_ASSIGN(SMSReceiver);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMS_SMS_RECEIVER_H_
