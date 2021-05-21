// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_hid_report_item.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ExecutionContext;
class HID;
class HIDCollectionInfo;
class ScriptPromiseResolver;
class ScriptState;

class MODULES_EXPORT HIDDevice
    : public EventTargetWithInlineData,
      public ExecutionContextLifecycleObserver,
      public ActiveScriptWrappable<HIDDevice>,
      public device::mojom::blink::HidConnectionClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  HIDDevice(HID* parent,
            device::mojom::blink::HidDeviceInfoPtr info,
            ExecutionContext* execution_context);
  ~HIDDevice() override;

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // device::mojom::blink::HidConnectionClient:
  void OnInputReport(uint8_t report_id, const Vector<uint8_t>& buffer) override;

  // Web-exposed interfaces:
  DEFINE_ATTRIBUTE_EVENT_LISTENER(inputreport, kInputreport)

  bool opened() const;
  uint16_t vendorId() const;
  uint16_t productId() const;
  String productName() const;
  const HeapVector<Member<HIDCollectionInfo>>& collections() const;

  ScriptPromise open(ScriptState*);
  ScriptPromise close(ScriptState*);
  ScriptPromise sendReport(ScriptState*,
                           uint8_t report_id,
                           const V8BufferSource* data);
  ScriptPromise sendFeatureReport(ScriptState*,
                                  uint8_t report_id,
                                  const V8BufferSource* data);
  ScriptPromise receiveFeatureReport(ScriptState*, uint8_t report_id);

  // ExecutionContextLifecycleObserver:
  void ContextDestroyed() override;

  // ActiveScriptWrappable:
  bool HasPendingActivity() const override;

  void UpdateDeviceInfo(device::mojom::blink::HidDeviceInfoPtr info);

  static HIDReportItem* ToHIDReportItem(
      const device::mojom::blink::HidReportItem& report_item);

  void Trace(Visitor*) const override;

 private:
  bool EnsureNoDeviceChangeInProgress(ScriptPromiseResolver* resolver) const;

  void OnServiceConnectionError();

  void FinishOpen(ScriptPromiseResolver*,
                  mojo::PendingRemote<device::mojom::blink::HidConnection>);
  void FinishClose(ScriptPromiseResolver*);
  void FinishSendReport(ScriptPromiseResolver*, bool success);
  void FinishReceiveReport(ScriptPromiseResolver*,
                           bool success,
                           uint8_t report_id,
                           const absl::optional<Vector<uint8_t>>&);
  void FinishSendFeatureReport(ScriptPromiseResolver*, bool success);
  void FinishReceiveFeatureReport(ScriptPromiseResolver*,
                                  bool success,
                                  const absl::optional<Vector<uint8_t>>&);

  void MarkRequestComplete(ScriptPromiseResolver*);

  Member<HID> parent_;
  device::mojom::blink::HidDeviceInfoPtr device_info_;
  HeapMojoRemote<device::mojom::blink::HidConnection> connection_;
  HeapMojoReceiver<device::mojom::blink::HidConnectionClient, HIDDevice>
      receiver_;
  HeapHashSet<Member<ScriptPromiseResolver>> device_requests_;
  HeapVector<Member<HIDCollectionInfo>> collections_;
  bool device_state_change_in_progress_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_DEVICE_H_
