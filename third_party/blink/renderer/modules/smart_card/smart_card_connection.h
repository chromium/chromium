// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_

#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class V8SmartCardDisposition;

class SmartCardConnection final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit SmartCardConnection(
      mojo::PendingRemote<device::mojom::blink::SmartCardConnection>,
      device::mojom::blink::SmartCardProtocol active_protocol,
      ExecutionContext*);

  // SmartCardConnection idl
  ScriptPromise disconnect(ScriptState* script_state,
                           ExceptionState& exception_state);
  ScriptPromise disconnect(ScriptState* script_state,
                           const V8SmartCardDisposition& disposition,
                           ExceptionState& exception_state);
  ScriptPromise transmit(ScriptState* script_state,
                         const DOMArrayPiece& send_buffer,
                         ExceptionState& exception_state);
  ScriptPromise status();
  ScriptPromise control(ScriptState* script_state,
                        uint32_t control_code,
                        const DOMArrayPiece& data,
                        ExceptionState& exception_state);
  ScriptPromise getAttribute(ScriptState* script_state,
                             uint32_t tag,
                             ExceptionState& exception_state);

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

 private:
  bool EnsureNoOperationInProgress(ExceptionState& exception_state) const;
  bool EnsureConnection(ExceptionState& exception_state) const;
  void OnDisconnectDone(ScriptPromiseResolver* resolver,
                        device::mojom::blink::SmartCardResultPtr result);
  void OnDataResult(ScriptPromiseResolver* resolver,
                    device::mojom::blink::SmartCardDataResultPtr result);
  void CloseMojoConnection();

  Member<ScriptPromiseResolver> ongoing_request_;
  HeapMojoRemote<device::mojom::blink::SmartCardConnection> connection_;
  device::mojom::blink::SmartCardProtocol active_protocol_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONNECTION_H_
