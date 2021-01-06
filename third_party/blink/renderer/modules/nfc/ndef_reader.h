// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"

namespace blink {

class NDEFScanOptions;
class NDEFWriteOptions;
class NFCProxy;
class ScriptPromiseResolver;
class StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

using NDEFMessageSource = StringOrArrayBufferOrArrayBufferViewOrNDEFMessageInit;

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
                      const NDEFMessageSource& write_message,
                      const NDEFWriteOptions* options,
                      ExceptionState& exception_state);

  void Trace(Visitor*) const override;

  // Called by NFCProxy for dispatching events.
  virtual void OnReading(const String& serial_number,
                         const device::mojom::blink::NDEFMessage&);
  virtual void OnReadingError(const String& message);

  // Called by NFCProxy for notification about connection error.
  void ReadOnMojoConnectionError();
  void WriteOnMojoConnectionError();

 private:
  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  NFCProxy* GetNfcProxy() const;

  void ReadAbort();
  void ReadOnRequestCompleted(device::mojom::blink::NDEFErrorPtr error);

  void WriteAbort();
  void WriteOnRequestCompleted(ScriptPromiseResolver* resolver,
                               device::mojom::blink::NDEFErrorPtr error);

  // Read Permission handling
  void ReadOnRequestPermission(const NDEFScanOptions* options,
                               mojom::blink::PermissionStatus status);

  // Write Permission handling
  void WriteOnRequestPermission(
      ScriptPromiseResolver* resolver,
      const NDEFWriteOptions* options,
      device::mojom::blink::NDEFMessagePtr ndef_message,
      mojom::blink::PermissionStatus status);

  // |scan_resolver_| is kept here to handle Mojo connection failures because in
  // that case the callback passed to Watch() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  Member<ScriptPromiseResolver> scan_resolver_;

  HeapMojoRemote<mojom::blink::PermissionService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      permission_service_;
  mojom::blink::PermissionService* GetPermissionService();

  // |write_requests_| are kept here to handle Mojo connection failures because
  // in that case the callback passed to Push() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  // This list will also be used by AbortSignal.
  HeapHashSet<Member<ScriptPromiseResolver>> write_requests_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
