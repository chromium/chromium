// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/hid/hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_associated_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_or_worker_scheduler.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class DOMWrapperWorld;
class ExecutionContext;
class HIDDeviceFilter;
class HIDDeviceRequestOptions;
class NavigatorBase;
class ScriptState;

class MODULES_EXPORT HID : public EventTarget,
                           public Supplement<NavigatorBase>,
                           public device::mojom::blink::HidManagerClient,
                           public HIDDevice::ServiceInterface {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  // Web-exposed getter for navigator.hid
  static HID* hid(NavigatorBase&);

  explicit HID(NavigatorBase&);
  ~HID() override;

  // EventTarget:
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // device::mojom::HidManagerClient:
  void DeviceAdded(device::mojom::blink::HidDeviceInfoPtr device_info) override;
  void DeviceRemoved(
      device::mojom::blink::HidDeviceInfoPtr device_info) override;
  void DeviceChanged(
      device::mojom::blink::HidDeviceInfoPtr device_info) override;

  // Web-exposed interfaces on hid object:
  DEFINE_ATTRIBUTE_EVENT_LISTENER(connect, kConnect)
  DEFINE_ATTRIBUTE_EVENT_LISTENER(disconnect, kDisconnect)
  ScriptPromise<IDLSequence<HIDDevice>> getDevices(ScriptState*,
                                                   ExceptionState&);
  ScriptPromise<IDLSequence<HIDDevice>>
  requestDevice(ScriptState*, const HIDDeviceRequestOptions*, ExceptionState&);

  // HIDDevice::ServiceInterface:
  void Connect(
      const String& device_guid,
      mojo::PendingRemote<device::mojom::blink::HidConnectionClient>
          connection_client,
      device::mojom::blink::HidManager::ConnectCallback callback) override;
  void Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
              mojom::blink::HidService::ForgetCallback callback) override;

  // Converts a HID device `filter` into the equivalent Mojo type and returns
  // it. CheckDeviceFilterValidity must be called first.
  static mojom::blink::HidDeviceFilterPtr ConvertDeviceFilter(
      const HIDDeviceFilter& filter);

  // Checks the validity of the given HIDDeviceFilter. Returns null string when
  // filter is valid or an error message when the filter is invalid.
  static String CheckDeviceFilterValidity(const HIDDeviceFilter& filter);

  void Trace(Visitor*) const override;

 protected:
  // EventTarget:
  void AddedEventListener(const AtomicString& event_type,
                          RegisteredEventListener&) override;

 private:
  friend class HIDTestHelper;
  // Helper class to wrap a world-specific HIDDevice cache.
  // Used when `WebHIDWorldIsolatedCache` feature is enabled to isolate
  // HIDDevice instances per DOMWrapperWorld.
  class HIDDeviceCache final : public GarbageCollected<HIDDeviceCache> {
   public:
    HIDDeviceCache() = default;
    HIDDeviceCache(const HIDDeviceCache&) = delete;
    HIDDeviceCache& operator=(const HIDDeviceCache&) = delete;
    HIDDeviceCache(HIDDeviceCache&&) = delete;
    HIDDeviceCache& operator=(HIDDeviceCache&&) = delete;

    void Trace(Visitor* visitor) const;
    HeapHashMap<String, WeakMember<HIDDevice>>& DeviceCache() {
      return device_cache_;
    }

   private:
    HeapHashMap<String, WeakMember<HIDDevice>> device_cache_;
  };

  HeapHashMap<String, WeakMember<HIDDevice>>& GetOrCreateWorldDeviceCache(
      DOMWrapperWorld& world);

  // Gets or creates a HIDDevice instance in the cache specific to `world`.
  HIDDevice* GetOrCreateDevice(
      DOMWrapperWorld& world,
      const device::mojom::blink::HidDeviceInfoPtr& info);
  // Helper that extracts the DOMWrapperWorld from `ScriptState` and calls the
  // world-specific GetOrCreateDevice overload.
  HIDDevice* GetOrCreateDevice(
      ScriptState*,
      const device::mojom::blink::HidDeviceInfoPtr& info);
  // Legacy fallback: gets or creates a device using the shared, non-isolated
  // cache (used when `WebHIDWorldIsolatedCache` is disabled).
  HIDDevice* GetOrCreateDevice(
      const device::mojom::blink::HidDeviceInfoPtr& info);

  // Helper to dispatch connect/disconnect events to all relevant worlds.
  void DispatchConnectionEvent(
      const AtomicString& event_type,
      device::mojom::blink::HidDeviceInfoPtr device_info);

  // Updates the device info in all world-specific caches that contain it.
  // Returns true if the device was found and updated in at least one cache.
  // This is only used when `WebHIDWorldIsolatedCache` is enabled.
  bool UpdateDeviceIfCached(
      const device::mojom::blink::HidDeviceInfoPtr& device_info);

  // Opens a connection to HidService, or does nothing if the connection is
  // already open.
  void EnsureServiceConnection();

  // Closes the connection to HidService and resolves any pending promises.
  void CloseServiceConnection();

  using HIDDeviceResolver = ScriptPromiseResolver<IDLSequence<HIDDevice>>;
  void FinishGetDevices(HIDDeviceResolver*,
                        Vector<device::mojom::blink::HidDeviceInfoPtr>);
  void FinishRequestDevice(HIDDeviceResolver*,
                           Vector<device::mojom::blink::HidDeviceInfoPtr>);

  HeapMojoRemote<mojom::blink::HidService> service_;
  HeapMojoAssociatedReceiver<device::mojom::blink::HidManagerClient, HID>
      receiver_;
  HeapHashSet<Member<HIDDeviceResolver>> get_devices_promises_;
  HeapHashSet<Member<HIDDeviceResolver>> request_device_promises_;
  // Map of V8 worlds to their respective HIDDeviceCache.
  // Used when `WebHIDWorldIsolatedCache` is enabled to ensure only one
  // HIDDevice instance represents each HID device inside a single global object
  // per world.
  HeapHashMap<WeakMember<DOMWrapperWorld>, Member<HIDDeviceCache>>
      device_caches_;

  // Map of device GUIDs to HIDDevice objects.
  // Legacy fallback: used when `WebHIDWorldIsolatedCache` is disabled.
  // Ensures only one HIDDevice instance represents each HID device inside a
  // single global object globally (shared across all worlds).
  HeapHashMap<String, WeakMember<HIDDevice>> device_cache_;
  std::optional<FrameOrWorkerScheduler::SchedulingAffectingFeatureHandle>
      feature_handle_for_scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_HID_HID_H_
