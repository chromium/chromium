// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_WRITER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_WRITER_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

class NDEFPushOptions;
class ExceptionState;
class ExecutionContext;
class ScriptPromise;
class StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

using NDEFMessageSource = StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

class NDEFWriter : public ScriptWrappable, public ContextClient {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(NDEFWriter);

 public:
  static NDEFWriter* Create(ExecutionContext*);

  explicit NDEFWriter(ExecutionContext*);
  ~NDEFWriter() override = default;

  void Trace(blink::Visitor*) override;

  // Pushes NDEFMessageSource asynchronously to NFC tag / peer.
  ScriptPromise push(ScriptState*,
                     const NDEFMessageSource&,
                     const NDEFPushOptions*,
                     ExceptionState&);

  // Called by NFCProxy for notification about connection error.
  void OnMojoConnectionError();

 private:
  void InitNfcProxyIfNeeded();
  void Abort(const String& target, ScriptPromiseResolver* resolver);
  void OnRequestCompleted(ScriptPromiseResolver* resolver,
                          device::mojom::blink::NDEFErrorPtr error);

  // |requests_| are kept here to handle Mojo connection failures because
  // in that case the callback passed to Push() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  // This list will also be used by AbortSignal.
  HeapHashSet<Member<ScriptPromiseResolver>> requests_;
  Member<NFCProxy> nfc_proxy_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_WRITER_H_
