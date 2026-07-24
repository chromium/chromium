// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_

#include <optional>

#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_advertising_event.h"
#include "third_party/blink/renderer/modules/bluetooth/bluetooth_device.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver_set.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class BluetoothDevice;
class BluetoothLEScan;
class BluetoothLEScanOptions;
class DOMWrapperWorld;
class ExceptionState;
class Navigator;
class RequestDeviceOptions;
class ScriptState;

class MODULES_EXPORT Bluetooth final
    : public EventTarget,
      public Supplement<Navigator>,
      public PageVisibilityObserver,
      public mojom::blink::WebBluetoothAdvertisementClient {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // IDL exposed as navigator.bluetooth
  static Bluetooth* bluetooth(Navigator&);

  explicit Bluetooth(Navigator&);
  ~Bluetooth() override;

  // IDL exposed bluetooth interface:
  ScriptPromise<IDLBoolean> getAvailability(ScriptState*, ExceptionState&);
  ScriptPromise<IDLSequence<BluetoothDevice>> getDevices(ScriptState*,
                                                         ExceptionState&);
  ScriptPromise<BluetoothDevice> requestDevice(ScriptState*,
                                               const RequestDeviceOptions*,
                                               ExceptionState&);

  ScriptPromise<BluetoothLEScan> requestLEScan(ScriptState*,
                                               const BluetoothLEScanOptions*,
                                               ExceptionState&);

  bool IsServiceBound() const { return service_.is_bound(); }
  mojom::blink::WebBluetoothService* Service() { return service_.get(); }

  // WebBluetoothAdvertisementClient
  void AdvertisingEvent(mojom::blink::WebBluetoothAdvertisingEventPtr) override;

  // EventTarget
  const AtomicString& InterfaceName() const override;
  ExecutionContext* GetExecutionContext() const override;

  // GC
  void Trace(Visitor*) const override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(advertisementreceived, kAdvertisementreceived)

  // PageVisibilityObserver
  void PageVisibilityChanged() override;

  void CancelScan(mojo::ReceiverId);
  bool IsScanActive(mojo::ReceiverId) const;

  BluetoothAdvertisingEvent* CreateBluetoothAdvertisingEvent(
      mojom::blink::WebBluetoothAdvertisingEventPtr advertising_event);

 private:
  friend class BluetoothTestHelper;

  // Helper class to wrap a world-specific BluetoothDevice cache.
  // Used when `WebBluetoothWorldIsolatedCache` feature is enabled to isolate
  // BluetoothDevice instances per DOMWrapperWorld.
  class BluetoothDeviceCache final
      : public GarbageCollected<BluetoothDeviceCache> {
   public:
    void Trace(Visitor* visitor) const;
    HeapHashMap<String, WeakMember<BluetoothDevice>>& DeviceCache() {
      return device_cache_;
    }

   private:
    HeapHashMap<String, WeakMember<BluetoothDevice>> device_cache_;
  };

  HeapHashMap<String, WeakMember<BluetoothDevice>>& GetOrCreateWorldDeviceCache(
      DOMWrapperWorld& world);

  // Gets or creates a BluetoothDevice instance in the cache specific to
  // `world`.
  BluetoothDevice* GetOrCreateBluetoothDevice(
      DOMWrapperWorld& world,
      const mojom::blink::WebBluetoothDevicePtr&,
      ExecutionContext*);
  // Helper that extracts the DOMWrapperWorld from `script_state` and calls the
  // world-specific GetOrCreateBluetoothDevice overload.
  BluetoothDevice* GetOrCreateBluetoothDevice(
      ScriptState*,
      const mojom::blink::WebBluetoothDevicePtr&,
      ExecutionContext*);
  // Legacy fallback: gets or creates a device using the shared, non-isolated
  // cache (used when `WebBluetoothWorldIsolatedCache` is disabled).
  BluetoothDevice* GetOrCreateBluetoothDevice(
      const mojom::blink::WebBluetoothDevicePtr&,
      ExecutionContext*);

  void GetDevicesCallback(ScriptPromiseResolver<IDLSequence<BluetoothDevice>>*,
                          Vector<mojom::blink::WebBluetoothDevicePtr>);

  void RequestDeviceCallback(ScriptPromiseResolver<BluetoothDevice>*,
                             mojom::blink::WebBluetoothResult,
                             mojom::blink::WebBluetoothDevicePtr);

  void RequestScanningCallback(
      ScriptPromiseResolver<BluetoothLEScan>*,
      mojo::ReceiverId,
      mojom::blink::WebBluetoothRequestLEScanOptionsPtr,
      mojom::blink::WebBluetoothResult);

  void EnsureServiceConnection(ExecutionContext*);

  // Map of V8 worlds to their respective BluetoothDeviceCache.
  // Used when `WebBluetoothWorldIsolatedCache` is enabled to ensure only one
  // BluetoothDevice instance represents each Bluetooth device inside a single
  // global object per world.
  HeapHashMap<WeakMember<DOMWrapperWorld>, Member<BluetoothDeviceCache>>
      device_caches_;

  // Map of device IDs to BluetoothDevice objects.
  // Legacy fallback: used when `WebBluetoothWorldIsolatedCache` is disabled.
  // Ensures only one BluetoothDevice instance represents each Bluetooth device
  // inside a single global object globally (shared across all worlds).
  HeapHashMap<String, Member<BluetoothDevice>> device_instance_map_;

  HeapMojoAssociatedReceiverSet<mojom::blink::WebBluetoothAdvertisementClient,
                                Bluetooth>
      client_receivers_;

  // Map of active scan Mojo ReceiverIds to their initiating V8 worlds.
  // Used when `WebBluetoothWorldIsolatedCache` is enabled to safely route
  // events from the frame-shared `WebBluetoothServiceImpl` Mojo pipe to the
  // correct isolated world.
  HeapHashMap<mojo::ReceiverId, WeakMember<DOMWrapperWorld>>
      client_receiver_world_map_;

  // Test-only fake receiver ID to bypass Mojo dispatch context in unit tests.
  std::optional<mojo::ReceiverId> fake_current_receiver_for_testing_;

  // HeapMojoRemote objects are associated with a ContextLifecycleNotifier and
  // cleaned up automatically when it is destroyed.
  HeapMojoRemote<mojom::blink::WebBluetoothService> service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_BLUETOOTH_BLUETOOTH_H_
