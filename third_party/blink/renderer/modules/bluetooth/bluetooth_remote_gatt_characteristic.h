// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_

#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/active_script_wrappable.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_remote_gatt_service.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BluetoothCharacteristicProperties;
class BluetoothDevice;
class DOMException;
class ExecutionContext;
class ScriptPromise;
class ScriptState;

// BluetoothRemoteGATTCharacteristic represents a GATT Characteristic, which is
// a basic data element that provides further information about a peripheral's
// service.
//
// Callbacks providing WebBluetoothRemoteGATTCharacteristicInit objects are
// handled by CallbackPromiseAdapter templatized with this class. See this
// class's "Interface required by CallbackPromiseAdapter" section and the
// CallbackPromiseAdapter class comments.
class BluetoothRemoteGATTCharacteristic final
    : public EventTargetWithInlineData,
      public ActiveScriptWrappable<BluetoothRemoteGATTCharacteristic>,
      public ContextLifecycleObserver,
      public mojom::blink::WebBluetoothCharacteristicClient {
  USING_PRE_FINALIZER(BluetoothRemoteGATTCharacteristic, Dispose);
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(BluetoothRemoteGATTCharacteristic);

 public:
  explicit BluetoothRemoteGATTCharacteristic(
      ExecutionContext*,
      mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr,
      BluetoothRemoteGATTService*,
      BluetoothDevice*);

  // Save value.
  void SetValue(DOMDataView*);

  // mojom::blink::WebBluetoothCharacteristicClient:
  void RemoteCharacteristicValueChanged(
      const WTF::Vector<uint8_t>& value) override;

  // ContextLifecycleObserver interface.
  void ContextDestroyed(ExecutionContext*) override;

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void Dispose();

  // EventTarget methods:
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // ActiveScriptWrappable methods:
  bool HasPendingActivity() const override;

  // Interface required by garbage collection.
  void Trace(blink::Visitor*) override;

  // IDL exposed interface:
  BluetoothRemoteGATTService* service() { return service_; }
  String uuid() { return characteristic_->uuid; }
  BluetoothCharacteristicProperties* properties() { return properties_; }
  DOMDataView* value() const { return value_; }
  ScriptPromise getDescriptor(ScriptState*,
                              const StringOrUnsignedLong& descriptor,
                              ExceptionState&);
  ScriptPromise getDescriptors(ScriptState*, ExceptionState&);
  ScriptPromise getDescriptors(ScriptState*,
                               const StringOrUnsignedLong& descriptor,
                               ExceptionState&);
  ScriptPromise readValue(ScriptState*);
  ScriptPromise writeValue(ScriptState*, const DOMArrayPiece&);
  ScriptPromise startNotifications(ScriptState*);
  ScriptPromise stopNotifications(ScriptState*);

  DEFINE_ATTRIBUTE_EVENT_LISTENER(characteristicvaluechanged,
                                  kCharacteristicvaluechanged)

 protected:
  // EventTarget overrides.
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  friend class BluetoothRemoteGATTDescriptor;

  BluetoothRemoteGATTServer* GetGatt() { return service_->device()->gatt(); }

  void ReadValueCallback(ScriptPromiseResolver*,
                         mojom::blink::WebBluetoothResult,
                         const base::Optional<Vector<uint8_t>>& value);
  void WriteValueCallback(ScriptPromiseResolver*,
                          const Vector<uint8_t>& value,
                          mojom::blink::WebBluetoothResult);
  void NotificationsCallback(ScriptPromiseResolver*,
                             mojom::blink::WebBluetoothResult);

  ScriptPromise GetDescriptorsImpl(ScriptState*,
                                   mojom::blink::WebBluetoothGATTQueryQuantity,
                                   const String& descriptor_uuid = String());

  void GetDescriptorsCallback(
      const String& requested_descriptor_uuid,
      const String& characteristic_instance_id,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      base::Optional<Vector<mojom::blink::WebBluetoothRemoteGATTDescriptorPtr>>
          descriptors);

  DOMException* CreateInvalidCharacteristicError();

  mojom::blink::WebBluetoothRemoteGATTCharacteristicPtr characteristic_;
  Member<BluetoothRemoteGATTService> service_;
  Member<BluetoothCharacteristicProperties> properties_;
  Member<DOMDataView> value_;
  Member<BluetoothDevice> device_;
  mojo::AssociatedReceiverSet<mojom::blink::WebBluetoothCharacteristicClient>
      receivers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_H_
