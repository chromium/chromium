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

class ExecutionContext;
class NFCProxy;
class NDEFScanOptions;
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
  ScriptPromise scan(ScriptState*, const NDEFScanOptions*, ExceptionState&);

  void Trace(Visitor*) const override;

  // Called by NFCProxy for dispatching events.
  virtual void OnReading(const String& serial_number,
                         const device::mojom::blink::NDEFMessage&);
  virtual void OnReadingError(const String& message);

  // Called by NFCProxy for notification about connection error.
  void OnMojoConnectionError();

 private:
  // ExecutionContextLifecycleObserver overrides.
  void ContextDestroyed() override;

  void Abort();

  NFCProxy* GetNfcProxy() const;

  void OnScanRequestCompleted(device::mojom::blink::NDEFErrorPtr error);

  // Permission handling
  void OnRequestPermission(const NDEFScanOptions* options,
                           mojom::blink::PermissionStatus status);
  mojom::blink::PermissionService* GetPermissionService();
  HeapMojoRemote<mojom::blink::PermissionService,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      permission_service_;

  // |resolver_| is kept here to handle Mojo connection failures because in that
  // case the callback passed to Watch() won't be called and
  // mojo::WrapCallbackWithDefaultInvokeIfNotRun() is forbidden in Blink.
  Member<ScriptPromiseResolver> resolver_;
  // To reject if there is already an ongoing scan.
  bool has_pending_scan_request_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NDEF_READER_H_
