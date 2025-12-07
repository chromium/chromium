// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_MANAGER_H_
#define EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/common/extension_id.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace device {
class UsbDevice;
}

namespace extensions {

// Stores information about a device saved with access granted.
class DevicePermissionEntry : public base::RefCounted<DevicePermissionEntry> {
 public:
  enum class Type {
    USB,
    HID,
  };

  explicit DevicePermissionEntry(const device::mojom::UsbDeviceInfo& device);

  explicit DevicePermissionEntry(const device::mojom::HidDeviceInfo& device);
  DevicePermissionEntry(Type type,
                        uint16_t vendor_id,
                        uint16_t product_id,
                        const std::u16string& serial_number,
                        const std::u16string& manufacturer_string,
                        const std::u16string& product_string,
                        const base::Time& last_used);

  // A persistent device is one that can be recognized when it is reconnected
  // and can therefore be remembered persistently by writing information about
  // it to ExtensionPrefs. Currently this means it has a serial number string.
  bool IsPersistent() const;

  // Convert the device to a serializable value, returns an is_none() value
  // if the entry is not persistent.
  base::Value::Dict ToValue() const;

  std::u16string GetPermissionMessageString() const;

  Type type() const { return type_; }
  uint16_t vendor_id() const { return vendor_id_; }
  uint16_t product_id() const { return product_id_; }
  const std::u16string& serial_number() const { return serial_number_; }
  const base::Time& last_used() const { return last_used_; }

  std::u16string GetManufacturer() const;
  std::u16string GetProduct() const;

 private:
  friend class base::RefCounted<DevicePermissionEntry>;
  friend class DevicePermissionsManager;

  ~DevicePermissionEntry();

  void set_last_used(const base::Time& last_used) { last_used_ = last_used; }

  // The device guid of a hid/USB device tracked by this entry.
  std::string device_guid_;
  // The type of device this entry represents.
  Type type_;
  // The vendor ID of this device.
  uint16_t vendor_id_;
  // The product ID of this device.
  uint16_t product_id_;
  // The serial number (possibly alphanumeric) of this device.
  std::u16string serial_number_;
  // The manufacturer string read from the device (optional).
  std::u16string manufacturer_string_;
  // The product string read from the device (optional).
  std::u16string product_string_;
  // The last time this device was used by the extension.
  base::Time last_used_;
};

// Stores device permissions associated with a particular extension.
class DevicePermissions {
 public:
  DevicePermissions(const DevicePermissions&) = delete;
  DevicePermissions& operator=(const DevicePermissions&) = delete;

  virtual ~DevicePermissions();

  // Attempts to find a permission entry matching the given device.
  scoped_refptr<DevicePermissionEntry> FindUsbDeviceEntry(
      scoped_refptr<device::UsbDevice> device) const;
  scoped_refptr<DevicePermissionEntry> FindUsbDeviceEntry(
      const device::mojom::UsbDeviceInfo& device) const;
  scoped_refptr<DevicePermissionEntry> FindHidDeviceEntry(
      const device::mojom::HidDeviceInfo& device) const;

  const std::set<scoped_refptr<DevicePermissionEntry>>& entries() const {
    return entries_;
  }

 private:
  friend class DevicePermissionsManager;

  // Reads permissions out of ExtensionPrefs.
  DevicePermissions(content::BrowserContext* context,
                    const ExtensionId& extension_id);

  std::set<scoped_refptr<DevicePermissionEntry>> entries_;
  std::map<std::string, scoped_refptr<DevicePermissionEntry>>
      ephemeral_usb_devices_;
  std::map<std::string, scoped_refptr<DevicePermissionEntry>>
      ephemeral_hid_devices_;
};

// Manages saved device permissions for all extensions.
class DevicePermissionsManager : public KeyedService {
 public:
  explicit DevicePermissionsManager(content::BrowserContext* context);
  ~DevicePermissionsManager() override;

  DevicePermissionsManager(const DevicePermissionsManager&) = delete;
  DevicePermissionsManager& operator=(const DevicePermissionsManager&) = delete;

  static DevicePermissionsManager* Get(content::BrowserContext* context);

  static std::u16string GetPermissionMessage(
      uint16_t vendor_id,
      uint16_t product_id,
      const std::u16string& manufacturer_string,
      const std::u16string& product_string,
      const std::u16string& serial_number,
      bool always_include_manufacturer);

  // The DevicePermissions object for a given extension.
  DevicePermissions* GetForExtension(const ExtensionId& extension_id);

  // Equivalent to calling GetForExtension and extracting the permission string
  // for each entry.
  std::vector<std::u16string> GetPermissionMessageStrings(
      const ExtensionId& extension_id) const;

  void AllowUsbDevice(const ExtensionId& extension_id,
                      const device::mojom::UsbDeviceInfo& device_info);
  void AllowHidDevice(const ExtensionId& extension_id,
                      const device::mojom::HidDeviceInfo& device);

  // Updates the "last used" timestamp on the given device entry and writes it
  // out to ExtensionPrefs.
  void UpdateLastUsed(const ExtensionId& extension_id,
                      scoped_refptr<DevicePermissionEntry> entry);

  // Revokes permission for the extension to access the given device.
  void RemoveEntry(const ExtensionId& extension_id,
                   scoped_refptr<DevicePermissionEntry> entry);

  // Revokes permission for an ephemeral hid/USB device by its guid.
  void RemoveEntryByDeviceGUID(DevicePermissionEntry::Type type,
                               const std::string& guid);

  // Revokes permission for the extension to access all allowed devices.
  void Clear(const ExtensionId& extension_id);

 private:
  friend class DevicePermissionsManagerFactory;
  FRIEND_TEST_ALL_PREFIXES(DevicePermissionsManagerTest, SuspendExtension);

  DevicePermissions* GetInternal(const ExtensionId& extension_id) const;

  base::ThreadChecker thread_checker_;
  raw_ptr<content::BrowserContext> context_;
  std::map<std::string, raw_ptr<DevicePermissions, CtnExperimental>>
      extension_id_to_device_permissions_;
};

class DevicePermissionsManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  DevicePermissionsManagerFactory(const DevicePermissionsManagerFactory&) =
      delete;
  DevicePermissionsManagerFactory& operator=(
      const DevicePermissionsManagerFactory&) = delete;

  static DevicePermissionsManager* GetForBrowserContext(
      content::BrowserContext* context);
  static DevicePermissionsManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DevicePermissionsManagerFactory>;

  DevicePermissionsManagerFactory();
  ~DevicePermissionsManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_MANAGER_H_
