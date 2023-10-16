// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVER_H_

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class BluetoothDevice;
class ExceptionState;
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

// BluetoothRemoteGATTServer provides a way to interact with a connected
// bluetooth peripheral.
class BluetoothRemoteGATTServer
    : public ScriptWrappable,
      public mojom::blink::WebBluetoothServerClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  BluetoothRemoteGATTServer(ExecutionContext*, BluetoothDevice*);

  // mojom::blink::WebBluetoothServerClient:
  void GATTServerDisconnected() override;

  // The Active Algorithms set is maintained so that disconnection, i.e.
  // disconnect() method or the device disconnecting by itself, can be detected
  // by algorithms. They check via RemoveFromActiveAlgorithms that their
  // resolvers is still in the set of active algorithms.
  //
  // Adds |resolver| to the set of Active Algorithms. CHECK-fails if
  // |resolver| was already added.
  void AddToActiveAlgorithms(ScriptPromiseResolver*);
  // Removes |resolver| from the set of Active Algorithms if it was in the set
  // and returns true, false otherwise.
  bool RemoveFromActiveAlgorithms(ScriptPromiseResolver*);

  // If gatt is connected then sets gatt.connected to false and disconnects.
  // This function only performs the necessary steps to ensure a device
  // disconnects therefore it should only be used when the object is being
  // garbage collected or the context is being destroyed.
  void DisconnectIfConnected();

  // Performs necessary cleanup when a device disconnects and fires
  // gattserverdisconnected event.
  void CleanupDisconnectedDeviceAndFireEvent();

  void DispatchDisconnected();

  // Interface required by Garbage Collectoin:
  void Trace(Visitor*) const override;

  // IDL exposed interface:
  BluetoothDevice* device() { return device_.Get(); }
  bool connected() { return connected_; }
  ScriptPromise connect(ScriptState*, ExceptionState&);
  void disconnect(ScriptState*, ExceptionState&);
  ScriptPromise getPrimaryService(ScriptState* script_state,
                                  const V8BluetoothServiceUUID* service,
                                  ExceptionState& exception_state);
  ScriptPromise getPrimaryServices(ScriptState* script_state,
                                   const V8BluetoothServiceUUID* service,
                                   ExceptionState& exception_state);
  ScriptPromise getPrimaryServices(ScriptState*, ExceptionState&);

 private:
  ScriptPromise GetPrimaryServicesImpl(
      ScriptState*,
      ExceptionState&,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      String service_uuid = String());

  void ConnectCallback(ScriptPromiseResolver*,
                       mojom::blink::WebBluetoothResult);
  void GetPrimaryServicesCallback(
      const String& requested_service_uuid,
      mojom::blink::WebBluetoothGATTQueryQuantity,
      ScriptPromiseResolver*,
      mojom::blink::WebBluetoothResult,
      absl::optional<Vector<mojom::blink::WebBluetoothRemoteGATTServicePtr>>
          services);

  // Contains a ScriptPromiseResolver corresponding to each active algorithm
  // using this server’s connection.
  HeapHashSet<Member<ScriptPromiseResolver>> active_algorithms_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  HeapMojoAssociatedReceiverSet<mojom::blink::WebBluetoothServerClient,
                                BluetoothRemoteGATTServer>
      client_receivers_;

  Member<BluetoothDevice> device_;
  bool connected_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_REMOTE_GATT_SERVER_H_
