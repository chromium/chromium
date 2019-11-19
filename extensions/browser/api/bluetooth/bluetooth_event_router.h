// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_

#include <map>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/api/bluetooth.h"
#include "extensions/common/api/bluetooth_private.h"

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
                             public content::NotificationObserver,
                             public ExtensionRegistryObserver {
 public:
  explicit BluetoothEventRouter(content::BrowserContext* context);
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
                             const std::string& extension_id,
                             const base::Closure& callback,
                             const base::Closure& error_callback);

  // Requests that the active discovery session that belongs to the extension
  // with id |extension_id| be terminated. |callback| is called, if the session
  // successfully ended. |error_callback| is called, if the adapter failed to
  // terminate the session or if no active discovery session exists for the
  // extension.
  void StopDiscoverySession(device::BluetoothAdapter* adapter,
                            const std::string& extension_id,
                            const base::Closure& callback,
                            const base::Closure& error_callback);

  // Requests that the filter associated with discovery session that belongs
  // to the extension with id |extension_id| be set to |discovery_filter|.
  // Callback is called, if the filter was successfully updated.
  // |error_callback| is called, if filter update failed.
  void SetDiscoveryFilter(
      std::unique_ptr<device::BluetoothDiscoveryFilter> discovery_filter,
      device::BluetoothAdapter* adapter,
      const std::string& extension_id,
      const base::Closure& callback,
      const base::Closure& error_callback);

  // Called when a bluetooth event listener is added.
  void OnListenerAdded(const EventListenerInfo& details);

  // Called when a bluetooth event listener is removed.
  void OnListenerRemoved(const EventListenerInfo& details);

  // Adds a pairing delegate for an extension.
  void AddPairingDelegate(const std::string& extension_id);

  // Removes the pairing delegate for an extension.
  void RemovePairingDelegate(const std::string& extension_id);

  // Returns the pairing delegate for an extension or NULL if it doesn't have a
  // pairing delegate.
  BluetoothApiPairingDelegate* GetPairingDelegate(
      const std::string& extension_id);

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

  // Overridden from content::NotificationObserver.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

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
                                 const std::string& extension_id,
                                 const base::Closure& callback,
                                 const base::Closure& error_callback);
  void AddPairingDelegateImpl(const std::string& extension_id);

  void OnAdapterInitialized(
      device::BluetoothAdapterFactory::AdapterCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void MaybeReleaseAdapter();
  void DispatchAdapterStateEvent();
  void DispatchDeviceEvent(events::HistogramValue histogram_value,
                           const std::string& event_name,
                           device::BluetoothDevice* device);
  void CleanUpForExtension(const std::string& extension_id);
  void CleanUpAllExtensions();
  void OnStartDiscoverySession(
      const std::string& extension_id,
      const base::Closure& callback,
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);

  void OnSetDiscoveryFilter(const std::string& extension_id,
                            const base::Closure& callback);

  content::BrowserContext* browser_context_;
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Map of listener id -> listener count.
  std::map<std::string, int> event_listener_count_;

  // A map that maps extension ids to BluetoothDiscoverySession pointers.
  typedef std::map<std::string, device::BluetoothDiscoverySession*>
      DiscoverySessionMap;
  DiscoverySessionMap discovery_session_map_;

  typedef std::map<std::string, device::BluetoothDiscoveryFilter*>
      PreSetFilterMap;

  // Maps an extension id to it's pre-set discovery filter.
  PreSetFilterMap pre_set_filter_map_;

  // Maps an extension id to its pairing delegate.
  typedef std::map<std::string, BluetoothApiPairingDelegate*>
      PairingDelegateMap;
  PairingDelegateMap pairing_delegate_map_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_{this};

  base::WeakPtrFactory<BluetoothEventRouter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothEventRouter);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_EVENT_ROUTER_H_
