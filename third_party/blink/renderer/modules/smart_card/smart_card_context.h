// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONTEXT_H_

#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_access_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_protocol.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {

class ScriptPromiseResolver;
class SmartCardReaderStateIn;

class SmartCardContext final : public ScriptWrappable,
                               public ExecutionContextClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SmartCardContext(mojo::PendingRemote<device::mojom::blink::SmartCardContext>,
                   ExecutionContext*);

  // SmartCardContext idl
  ScriptPromise listReaders(ScriptState* script_state,
                            ExceptionState& exception_state);

  ScriptPromise getStatusChange(
      ScriptState* script_state,
      const HeapVector<Member<SmartCardReaderStateIn>>& reader_states,
      AbortSignal* signal,
      ExceptionState& exception_state);

  ScriptPromise connect(ScriptState* script_state,
                        const String& reader_name,
                        V8SmartCardAccessMode access_mode,
                        const Vector<V8SmartCardProtocol>& preferred_protocols,
                        ExceptionState& exception_state);

  ScriptPromise connect(ScriptState* script_state,
                        const String& reader_name,
                        V8SmartCardAccessMode access_mode,
                        ExceptionState& exception_state);

  // ScriptWrappable overrides
  void Trace(Visitor*) const override;

 private:
  class GetStatusChangeAbortAlgorithm;

  void CloseMojoConnection();
  bool EnsureNoOperationInProgress(ExceptionState& exception_state) const;
  bool EnsureMojoConnection(ExceptionState& exception_state) const;
  void OnListReadersDone(ScriptPromiseResolver* resolver,
                         device::mojom::blink::SmartCardListReadersResultPtr);
  void OnGetStatusChangeDone(
      ScriptPromiseResolver* resolver,
      device::mojom::blink::SmartCardStatusChangeResultPtr result);
  void OnCancelDone(device::mojom::blink::SmartCardResultPtr result);
  void OnConnectDone(ScriptPromiseResolver* resolver,
                     device::mojom::blink::SmartCardConnectResultPtr result);
  void AbortGetStatusChange();
  void ResetAbortSignal();

  HeapMojoRemote<device::mojom::blink::SmartCardContext> scard_context_;
  Member<ScriptPromiseResolver> list_readers_request_;
  Member<ScriptPromiseResolver> connect_request_;

  Member<AbortSignal> get_status_change_abort_signal_;
  Member<AbortSignal::AlgorithmHandle> get_status_change_abort_handle_;
  Member<ScriptPromiseResolver> get_status_change_request_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SMART_CARD_SMART_CARD_CONTEXT_H_
