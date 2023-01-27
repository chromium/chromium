// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_USB_DEVICE_MANAGER_H_
#define EXTENSIONS_BROWSER_API_USB_USB_DEVICE_MANAGER_H_

#include <map>
#include <string>
#include <vector>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/usb.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace extensions {

// A BrowserContext-scoped object which is registered as an observer of the
// EventRouter and UsbDeviceManager in order to manage the connection to the
// Device Service and generate device add/remove events.
class UsbDeviceManager : public BrowserContextKeyedAPI,
                         public EventRouter::Observer,
                         public device::mojom::UsbDeviceManagerClient {
 public:
  UsbDeviceManager(const UsbDeviceManager&) = delete;
  UsbDeviceManager& operator=(const UsbDeviceManager&) = delete;

  static UsbDeviceManager* Get(content::BrowserContext* browser_context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<UsbDeviceManager>* GetFactoryInstance();

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnDeviceAdded(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceRemoved(const device::mojom::UsbDeviceInfo&);
    virtual void OnDeviceManagerConnectionError();
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns an ID for this device GUID. If the GUID is unknown to the
  // UsbGuidMap a new ID is generated for it.
  int GetIdFromGuid(const std::string& guid);

  // Looks up a device GUID for a given extensions USB device ID. If the ID is
  // unknown (e.g., the corresponding device was unplugged), this returns
  // |false|; otherwise it returns |true|.
  bool GetGuidFromId(int id, std::string* guid);

  // Populates an instance of the chrome.usb.Device object from the given
  // device.
  void GetApiDevice(const device::mojom::UsbDeviceInfo& device_in,
                    api::usb::Device* device_out);

  // Forward UsbDeviceManager methods.
  void GetDevices(device::mojom::UsbDeviceManager::GetDevicesCallback callback);
  void GetDevice(
      const std::string& guid,
      mojo::PendingReceiver<device::mojom::UsbDevice> device_receiver);

  const device::mojom::UsbDeviceInfo* GetDeviceInfo(const std::string& guid);
  bool UpdateActiveConfig(const std::string& guid, uint8_t config_value);

#if BUILDFLAG(IS_CHROMEOS)
  void CheckAccess(
      const std::string& guid,
      device::mojom::UsbDeviceManager::CheckAccessCallback callback);
#endif  // BUILDFLAG(IS_CHROMEOS)

  void EnsureConnectionWithDeviceManager();

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> fake_device_manager);

 private:
  friend class BrowserContextKeyedAPIFactory<UsbDeviceManager>;

  explicit UsbDeviceManager(content::BrowserContext* context);
  ~UsbDeviceManager() override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UsbDeviceManager"; }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;

  // UsbService::Observer implementation.
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device_info) override;

  void SetUpDeviceManagerConnection();
  void InitDeviceList(std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  void OnDeviceManagerConnectionError();

  // Broadcasts a device add or remove event for the given device.
  void DispatchEvent(const std::string& event_name,
                     const device::mojom::UsbDeviceInfo& device_info);

  const raw_ptr<content::BrowserContext> browser_context_;

  // Legacy integer IDs are used in USB extensions API so we need to maps USB
  // device GUIDs to integer IDs.
  int next_id_ = 0;
  std::map<std::string, int> guid_to_id_map_;
  std::map<int, std::string> id_to_guid_map_;

  bool is_initialized_ = false;
  base::queue<device::mojom::UsbDeviceManager::GetDevicesCallback>
      pending_get_devices_requests_;
  std::map<std::string, device::mojom::UsbDeviceInfoPtr> devices_;

  // Connection to |device_manager_instance_|.
  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<UsbDeviceManager> weak_factory_{this};
};

template <>
void BrowserContextKeyedAPIFactory<
    UsbDeviceManager>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_USB_DEVICE_MANAGER_H_
