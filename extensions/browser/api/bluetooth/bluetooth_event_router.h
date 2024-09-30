// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/api/bluetooth_private.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
}

namespace device {

class BluetoothDevice;
class BluetoothDiscoverySession;

}  // namespace device

namespace extensions {
class BluetoothApiPairingDelegate;
struct EventListenerInfo;

class BluetoothEventRouter : public device::BluetoothAdapter::Observer,
                             public ExtensionRegistryObserver,
                             public ExtensionHostRegistry::Observer {
 public:
  explicit BluetoothEventRouter(content::BrowserContext* context);

  BluetoothEventRouter(const BluetoothEventRouter&) = delete;
  BluetoothEventRouter& operator=(const BluetoothEventRouter&) = delete;

  ~BluetoothEventRouter() override;

  // Returns true if adapter_ has been initialized for testing or bluetooth
  // adapter is available for the current platform.
  bool IsBluetoothSupported() const;

  void GetAdapter(device::BluetoothAdapterFactory::AdapterCallback callback);

  // Requests that a new device discovery session be initiated for extension
  // with id |extension_id|. |callback| is called, if a session has been
  // initiated. |error_callback| is called, if the adapter failed to initiate
  // the session or if an active session already exists for the extension.
  void StartDiscoverySession(device::BluetoothAdapter* adapter,
                             const ExtensionId& extension_id,
                             base::OnceClosure callback,
                             base::OnceClosure error_callback);

  // Requests that the active discovery session that belongs to the extension
  // with id |extension_id| be terminated. |callback| is called, if the session
  // successfully ended. |error_callback| is called, if the adapter failed to
  // terminate the session or if no active discovery session exists for the
  // extension.
  void StopDiscoverySession(device::BluetoothAdapter* adapter,
                            const ExtensionId& extension_id,
                            base::OnceClosure callback,
                            base::OnceClosure error_callback);

  // Requests that the filter associated with discovery session that belongs
  // to the extension with id |extension_id| be set to |discovery_filter|.
  // |callback| is called, if the filter was successfully updated.
  // |error_callback| is called, if filter update failed.
  void SetDiscoveryFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      device::BluetoothAdapter* adapter,
      const ExtensionId& extension_id,
      base::OnceClosure callback,
      base::OnceClosure error_callback);

  // Called when a bluetooth event listener is added.
  void OnListenerAdded(const EventListenerInfo& details);

  // Called when a bluetooth event listener is removed.
  void OnListenerRemoved(const EventListenerInfo& details);

  // Adds a pairing delegate for an extension.
  void AddPairingDelegate(const ExtensionId& extension_id);

  // Removes the pairing delegate for an extension.
  void RemovePairingDelegate(const ExtensionId& extension_id);

  // Returns the pairing delegate for an extension or NULL if it doesn't have a
  // pairing delegate.
  BluetoothApiPairingDelegate* GetPairingDelegate(
      const ExtensionId& extension_id);

  // Exposed for testing.
  void SetAdapterForTest(device::BluetoothAdapter* adapter) {
    adapter_ = adapter;
  }

  // Override from device::BluetoothAdapter::Observer.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool has_power) override;
  void AdapterDiscoveringChanged(device::BluetoothAdapter* adapter,
                                 bool discovering) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceAddressChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            const std::string& old_address) override;

  // Overridden from ExtensionRegistryObserver.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothEventRouter"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  void StartDiscoverySessionImpl(device::BluetoothAdapter* adapter,
                                 const ExtensionId& extension_id,
                                 base::OnceClosure callback,
                                 base::OnceClosure error_callback);
  void AddPairingDelegateImpl(const ExtensionId& extension_id);

  void OnAdapterInitialized(
      device::BluetoothAdapterFactory::AdapterCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void MaybeReleaseAdapter();
  void DispatchAdapterStateEvent();
  void DispatchDeviceEvent(events::HistogramValue histogram_value,
                           const std::string& event_name,
                           device::BluetoothDevice* device);
  void CleanUpForExtension(const ExtensionId& extension_id);
  void CleanUpAllExtensions();
  void OnStartDiscoverySession(
      const ExtensionId& extension_id,
      base::OnceClosure callback,
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostDestroyed(content::BrowserContext* browser_context,
                                ExtensionHost* host) override;

  raw_ptr<content::BrowserContext> browser_context_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Map of listener id -> listener count.
  std::map<std::string, int> event_listener_count_;

  // A map that maps extension ids to BluetoothDiscoverySession pointers.
  typedef std::map<std::string,
                   raw_ptr<device::BluetoothDiscoverySession, CtnExperimental>>
      DiscoverySessionMap;
  DiscoverySessionMap discovery_session_map_;

  typedef std::map<std::string,
                   raw_ptr<device::BluetoothDiscoveryFilter, CtnExperimental>>
      PreSetFilterMap;

  // Maps an extension id to it's pre-set discovery filter.
  PreSetFilterMap pre_set_filter_map_;

  // Maps an extension id to its pairing delegate.
  typedef std::map<std::string,
                   raw_ptr<BluetoothApiPairingDelegate, CtnExperimental>>
      PairingDelegateMap;
  PairingDelegateMap pairing_delegate_map_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      extension_host_registry_observation_{this};

  base::WeakPtrFactory<BluetoothEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
