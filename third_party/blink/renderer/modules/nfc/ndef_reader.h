// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_

#include <memory>

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class AbortSignal;
class NDEFScanOptions;
class NDEFMakeReadOnlyOptions;
class NDEFWriteOptions;
class NFCProxy;
class ScopedAbortState;
class ScriptPromiseResolver;

class MODULES_EXPORT NDEFReader : public EventTargetWithInlineData,
                                  public ActiveScriptWrappable<NDEFReader>,
                                  public ExecutionContextLifecycleObserver {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static NDEFReader* Create(ExecutionContext*);

  NDEFReader(ExecutionContext*);
  ~NDEFReader() override;

  // EventTarget overrides.
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable overrides.
  bool HasPendingActivity() const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(reading, kReading)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readingerror, kReadingerror)

  // Scan from an NFC tag.
  ScriptPromise scan(ScriptState* script_state,
                     const NDEFScanOptions* options,
                     ExceptionState& exception_state);

  // Write NDEFMessageSource asynchronously to NFC tag.
  ScriptPromise write(ScriptState* script_state,
                      const V8NDEFMessageSource* write_message,
                      const NDEFWriteOptions* options,
                      ExceptionState& exception_state);

  // Make NFC tag permanently read-only.
  ScriptPromise makeReadOnly(ScriptState* script_state,
                             const NDEFMakeReadOnlyOptions* options,
                             ExceptionState& exception_state);

  void Trace(Visitor*) const override;

  // Called by NFCProxy for dispatching events.
  virtual void OnReading(const String& serial_number,
                         const device::mojom::blink::NDEFMessage&);
  virtual void OnReadingError(const String& message);

  // Called by NFCProxy for notification about connection error.
  void ReadOnMojoConnectionError();
  void WriteOnMojoConnectionError();
  void MakeReadOnlyOnMojoConnectionError();

 private:
  class ReadAbortAlgorithm;
  class WriteAbortAlgorithm;
  class MakeReadOnlyAbortAlgorithm;

  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  NFCProxy* GetNfcProxy() const;

  void ReadAbort(AbortSignal* signal);
  void ReadOnRequestCompleted(device::mojom::blink::NDEFErrorPtr error);

  void WriteAbort(AbortSignal* signal);
  void WriteOnRequestCompleted(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<ScopedAbortState> scoped_abort_state,
      device::mojom::blink::NDEFErrorPtr error);

  void MakeReadOnlyAbort(AbortSignal* signal);
  void MakeReadOnlyOnRequestCompleted(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<ScopedAbortState> scoped_abort_state,
      device::mojom::blink::NDEFErrorPtr error);

  // Read Permission handling
  void ReadOnRequestPermission(const NDEFScanOptions* options,
                               mojom::blink::PermissionStatus status);

  // Write Permission handling
  void WriteOnRequestPermission(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<ScopedAbortState> scoped_abort_state,
      const NDEFWriteOptions* options,
      device::mojom::blink::NDEFMessagePtr ndef_message,
      mojom::blink::PermissionStatus status);

  // Make read-only permission handling
  void MakeReadOnlyOnRequestPermission(
      ScriptPromiseResolver* resolver,
      std::unique_ptr<ScopedAbortState> scoped_abort_state,
      const NDEFMakeReadOnlyOptions* options,
      mojom::blink::PermissionStatus status);

  // |scan_resolver_| is kept here to handle Mojo connection failures because in
  // that case the callback passed to Watch() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  Member<ScriptPromiseResolver> scan_resolver_;
  Member<AbortSignal> scan_signal_;
  // The abort algorithm added during scan() needs to be valid while reading,
  // after resolving the scan() promise.
  Member<AbortSignal::AlgorithmHandle> scan_abort_handle_;

  HeapMojoRemote<mojom::blink::PermissionService> permission_service_;
  mojom::blink::PermissionService* GetPermissionService();

  // |write_requests_| are kept here to handle Mojo connection failures because
  // in that case the callback passed to Push() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  HeapHashSet<Member<ScriptPromiseResolver>> write_requests_;
  Member<AbortSignal> write_signal_;

  // |make_read_only_requests_| are kept here to handle Mojo connection failures
  // because in that case the callback passed to MakeReadOnly() won't be called
  // and mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  HeapHashSet<Member<ScriptPromiseResolver>> make_read_only_requests_;
  Member<AbortSignal> make_read_only_signal_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
