// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_
#define THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_

#include "services/device/public/mojom/hid.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/chromeos/system_extensions/hid/cros_hid.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/modules/hid/hid_device.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HIDDeviceRequestOptions;
class ScriptPromiseResolver;

class CrosHID : public ScriptWrappable,
                public Supplement<ExecutionContext>,
                public ExecutionContextClient,
                public HIDDevice::ServiceInterface {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static const char kSupplementName[];

  static CrosHID& From(ExecutionContext&);

  explicit CrosHID(ExecutionContext&);

  void Trace(Visitor*) const override;

  ScriptPromise accessDevice(ScriptState* script_state,
                             const HIDDeviceRequestOptions*);

  // HIDDevice::ServiceInterface:
  void Connect(
      const String& device_guid,
      mojo::PendingRemote<device::mojom::blink::HidConnectionClient>
          connection_client,
      device::mojom::blink::HidManager::ConnectCallback callback) override;
  void Forget(device::mojom::blink::HidDeviceInfoPtr device_info,
              mojom::blink::HidService::ForgetCallback callback) override;

 private:
  // Returns the remote for communication with the browser's HID
  // implementation. May return null in error cases.
  mojom::blink::CrosHID* GetCrosHIDOrNull();

  void OnAccessDevicesResponse(
      ScriptPromiseResolver* resolver,
      WTF::Vector<device::mojom::blink::HidDeviceInfoPtr> device_infos);

  // Returns the HIDDevice matching `info` from `device_cache_`. If the device
  // is not in the cache, a new device is created and added to the cache.
  HIDDevice* GetOrCreateDevice(device::mojom::blink::HidDeviceInfoPtr info);

  HeapMojoRemote<mojom::blink::CrosHID> cros_hid_;
  HeapHashMap<String, WeakMember<HIDDevice>> device_cache_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_EXTENSIONS_CHROMEOS_SYSTEM_EXTENSIONS_HID_CROS_HID_H_
