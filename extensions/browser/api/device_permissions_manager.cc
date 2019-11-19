// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/device_permissions_manager.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/value_builder.h"
#include "extensions/strings/grit/extensions_strings.h"
#include "services/device/public/cpp/usb/usb_ids.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

using content::BrowserContext;
using content::BrowserThread;
using extensions::APIPermission;
using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;

namespace {

// Preference keys

// The device that the app has permission to access.
const char kDevices[] = "devices";

// The type of device saved.
const char kDeviceType[] = "type";

// Type identifier for USB devices.
const char kDeviceTypeUsb[] = "usb";

// Type identifier for HID devices.
const char kDeviceTypeHid[] = "hid";

// The vendor ID of the device that the app had permission to access.
const char kDeviceVendorId[] = "vendor_id";

// The product ID of the device that the app had permission to access.
const char kDeviceProductId[] = "product_id";

// The serial number of the device that the app has permission to access.
const char kDeviceSerialNumber[] = "serial_number";

// The manufacturer string read from the device that the app has permission to
// access.
const char kDeviceManufacturerString[] = "manufacturer_string";

// The product string read from the device that the app has permission to
// access.
const char kDeviceProductString[] = "product_string";

// Serialized timestamp of the last time when the device was opened by the app.
const char kDeviceLastUsed[] = "last_used_time";

// Converts a DevicePermissionEntry::Type to a string for the prefs file.
const char* TypeToString(DevicePermissionEntry::Type type) {
  switch (type) {
    case DevicePermissionEntry::Type::USB:
      return kDeviceTypeUsb;
    case DevicePermissionEntry::Type::HID:
      return kDeviceTypeHid;
  }
  NOTREACHED();
  return "";
}

// Persists a DevicePermissionEntry in ExtensionPrefs.
void SaveDevicePermissionEntry(BrowserContext* context,
                               const std::string& extension_id,
                               scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    devices = update.Create();
  }

  std::unique_ptr<base::Value> device_entry(entry->ToValue());
  DCHECK(devices->Find(*device_entry) == devices->end());
  devices->Append(std::move(device_entry));
}

bool MatchesDevicePermissionEntry(const base::DictionaryValue* value,
                                  scoped_refptr<DevicePermissionEntry> entry) {
  std::string type;
  if (!value->GetStringWithoutPathExpansion(kDeviceType, &type) ||
      type != TypeToString(entry->type())) {
    return false;
  }
  int vendor_id;
  if (!value->GetIntegerWithoutPathExpansion(kDeviceVendorId, &vendor_id) ||
      vendor_id != entry->vendor_id()) {
    return false;
  }
  int product_id;
  if (!value->GetIntegerWithoutPathExpansion(kDeviceProductId, &product_id) ||
      product_id != entry->product_id()) {
    return false;
  }
  base::string16 serial_number;
  if (!value->GetStringWithoutPathExpansion(kDeviceSerialNumber,
                                            &serial_number) ||
      serial_number != entry->serial_number()) {
    return false;
  }
  return true;
}

// Updates the timestamp stored in ExtensionPrefs for the given
// DevicePermissionEntry.
void UpdateDevicePermissionEntry(BrowserContext* context,
                                 const std::string& extension_id,
                                 scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    return;
  }

  for (size_t i = 0; i < devices->GetSize(); ++i) {
    base::DictionaryValue* dict_value;
    if (!devices->GetDictionary(i, &dict_value)) {
      continue;
    }
    if (!MatchesDevicePermissionEntry(dict_value, entry)) {
      continue;
    }
    devices->Set(i, entry->ToValue());
    break;
  }
}

// Removes the given DevicePermissionEntry from ExtensionPrefs.
void RemoveDevicePermissionEntry(BrowserContext* context,
                                 const std::string& extension_id,
                                 scoped_refptr<DevicePermissionEntry> entry) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  ExtensionPrefs::ScopedListUpdate update(prefs, extension_id, kDevices);
  base::ListValue* devices = update.Get();
  if (!devices) {
    return;
  }

  for (size_t i = 0; i < devices->GetSize(); ++i) {
    base::DictionaryValue* dict_value;
    if (!devices->GetDictionary(i, &dict_value)) {
      continue;
    }
    if (!MatchesDevicePermissionEntry(dict_value, entry)) {
      continue;
    }
    devices->Remove(i, nullptr);
    break;
  }
}

// Clears all DevicePermissionEntries for the app from ExtensionPrefs.
void ClearDevicePermissionEntries(ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  prefs->UpdateExtensionPref(extension_id, kDevices, nullptr);
}

scoped_refptr<DevicePermissionEntry> ReadDevicePermissionEntry(
    const base::DictionaryValue* entry) {
  int vendor_id;
  if (!entry->GetIntegerWithoutPathExpansion(kDeviceVendorId, &vendor_id) ||
      vendor_id < 0 || vendor_id > UINT16_MAX) {
    return nullptr;
  }

  int product_id;
  if (!entry->GetIntegerWithoutPathExpansion(kDeviceProductId, &product_id) ||
      product_id < 0 || product_id > UINT16_MAX) {
    return nullptr;
  }

  base::string16 serial_number;
  if (!entry->GetStringWithoutPathExpansion(kDeviceSerialNumber,
                                            &serial_number)) {
    return nullptr;
  }

  base::string16 manufacturer_string;
  // Ignore failure as this string is optional.
  entry->GetStringWithoutPathExpansion(kDeviceManufacturerString,
                                       &manufacturer_string);

  base::string16 product_string;
  // Ignore failure as this string is optional.
  entry->GetStringWithoutPathExpansion(kDeviceProductString, &product_string);

  // If a last used time is not stored in ExtensionPrefs last_used.is_null()
  // will be true.
  std::string last_used_str;
  int64_t last_used_i64 = 0;
  base::Time last_used;
  if (entry->GetStringWithoutPathExpansion(kDeviceLastUsed, &last_used_str) &&
      base::StringToInt64(last_used_str, &last_used_i64)) {
    last_used = base::Time::FromInternalValue(last_used_i64);
  }

  std::string type;
  if (!entry->GetStringWithoutPathExpansion(kDeviceType, &type)) {
    return nullptr;
  }

  if (type == kDeviceTypeUsb) {
    return base::MakeRefCounted<DevicePermissionEntry>(
        DevicePermissionEntry::Type::USB, vendor_id, product_id, serial_number,
        manufacturer_string, product_string, last_used);
  } else if (type == kDeviceTypeHid) {
    return base::MakeRefCounted<DevicePermissionEntry>(
        DevicePermissionEntry::Type::HID, vendor_id, product_id, serial_number,
        base::string16(), product_string, last_used);
  }
  return nullptr;
}

// Returns all DevicePermissionEntries for the app.
std::set<scoped_refptr<DevicePermissionEntry>> GetDevicePermissionEntries(
    ExtensionPrefs* prefs,
    const std::string& extension_id) {
  std::set<scoped_refptr<DevicePermissionEntry>> result;
  const base::ListValue* devices = NULL;
  if (!prefs->ReadPrefAsList(extension_id, kDevices, &devices)) {
    return result;
  }

  for (const auto& entry : *devices) {
    const base::DictionaryValue* entry_dict;
    if (entry.GetAsDictionary(&entry_dict)) {
      scoped_refptr<DevicePermissionEntry> device_entry =
          ReadDevicePermissionEntry(entry_dict);
      if (entry_dict) {
        result.insert(device_entry);
      }
    }
  }
  return result;
}

}  // namespace

DevicePermissionEntry::DevicePermissionEntry(
    const device::mojom::UsbDeviceInfo& device)
    : device_guid_(device.guid),
      type_(Type::USB),
      vendor_id_(device.vendor_id),
      product_id_(device.product_id) {
  if (device.serial_number)
    serial_number_ = *device.serial_number;

  if (device.manufacturer_name)
    manufacturer_string_ = *device.manufacturer_name;

  if (device.product_name)
    product_string_ = *device.product_name;
}

DevicePermissionEntry::DevicePermissionEntry(
    const device::mojom::HidDeviceInfo& device)
    : device_guid_(device.guid),
      type_(Type::HID),
      vendor_id_(device.vendor_id),
      product_id_(device.product_id),
      serial_number_(base::UTF8ToUTF16(device.serial_number)),
      product_string_(base::UTF8ToUTF16(device.product_name)) {}

DevicePermissionEntry::DevicePermissionEntry(
    Type type,
    uint16_t vendor_id,
    uint16_t product_id,
    const base::string16& serial_number,
    const base::string16& manufacturer_string,
    const base::string16& product_string,
    const base::Time& last_used)
    : type_(type),
      vendor_id_(vendor_id),
      product_id_(product_id),
      serial_number_(serial_number),
      manufacturer_string_(manufacturer_string),
      product_string_(product_string),
      last_used_(last_used) {
}

DevicePermissionEntry::~DevicePermissionEntry() {
}

bool DevicePermissionEntry::IsPersistent() const {
  return !serial_number_.empty();
}

std::unique_ptr<base::Value> DevicePermissionEntry::ToValue() const {
  if (!IsPersistent()) {
    return nullptr;
  }

  DCHECK(!serial_number_.empty());
  std::unique_ptr<base::DictionaryValue> entry_dict(
      DictionaryBuilder()
          .Set(kDeviceType, TypeToString(type_))
          .Set(kDeviceVendorId, vendor_id_)
          .Set(kDeviceProductId, product_id_)
          .Set(kDeviceSerialNumber, serial_number_)
          .Build());

  if (!manufacturer_string_.empty()) {
    entry_dict->SetKey(kDeviceManufacturerString,
                       base::Value(manufacturer_string_));
  }
  if (!product_string_.empty()) {
    entry_dict->SetKey(kDeviceProductString, base::Value(product_string_));
  }
  if (!last_used_.is_null()) {
    entry_dict->SetKey(
        kDeviceLastUsed,
        base::Value(base::NumberToString(last_used_.ToInternalValue())));
  }

  return std::move(entry_dict);
}

base::string16 DevicePermissionEntry::GetPermissionMessageString() const {
  return DevicePermissionsManager::GetPermissionMessage(
      vendor_id_, product_id_, manufacturer_string_, product_string_,
      serial_number_, type_ == Type::USB);
}

DevicePermissions::~DevicePermissions() {
}

scoped_refptr<DevicePermissionEntry> DevicePermissions::FindUsbDeviceEntry(
    const device::mojom::UsbDeviceInfo& device) const {
  const auto& ephemeral_device_entry = ephemeral_usb_devices_.find(device.guid);
  if (ephemeral_device_entry != ephemeral_usb_devices_.end()) {
    return ephemeral_device_entry->second;
  }

  if (!device.serial_number || device.serial_number->empty()) {
    return nullptr;
  }

  for (const auto& entry : entries_) {
    if (entry->IsPersistent() && entry->vendor_id() == device.vendor_id &&
        entry->product_id() == device.product_id &&
        entry->serial_number() == *device.serial_number) {
      return entry;
    }
  }
  return nullptr;
}

scoped_refptr<DevicePermissionEntry> DevicePermissions::FindHidDeviceEntry(
    const device::mojom::HidDeviceInfo& device) const {
  const auto& ephemeral_device_entry = ephemeral_hid_devices_.find(device.guid);
  if (ephemeral_device_entry != ephemeral_hid_devices_.end()) {
    return ephemeral_device_entry->second;
  }

  if (device.serial_number.empty()) {
    return nullptr;
  }

  base::string16 serial_number = base::UTF8ToUTF16(device.serial_number);
  for (const auto& entry : entries_) {
    if (entry->IsPersistent() && entry->vendor_id() == device.vendor_id &&
        entry->product_id() == device.product_id &&
        entry->serial_number() == serial_number) {
      return entry;
    }
  }
  return nullptr;
}

DevicePermissions::DevicePermissions(BrowserContext* context,
                                     const std::string& extension_id) {
  ExtensionPrefs* prefs = ExtensionPrefs::Get(context);
  entries_ = GetDevicePermissionEntries(prefs, extension_id);
}

// static
DevicePermissionsManager* DevicePermissionsManager::Get(
    BrowserContext* context) {
  return DevicePermissionsManagerFactory::GetForBrowserContext(context);
}

// static
base::string16 DevicePermissionsManager::GetPermissionMessage(
    uint16_t vendor_id,
    uint16_t product_id,
    const base::string16& manufacturer_string,
    const base::string16& product_string,
    const base::string16& serial_number,
    bool always_include_manufacturer) {
  base::string16 product = product_string;
  if (product.empty()) {
    const char* product_name =
        device::UsbIds::GetProductName(vendor_id, product_id);
    if (product_name) {
      product = base::UTF8ToUTF16(product_name);
    }
  }

  base::string16 manufacturer = manufacturer_string;
  if (manufacturer_string.empty()) {
    const char* vendor_name = device::UsbIds::GetVendorName(vendor_id);
    if (vendor_name) {
      manufacturer = base::UTF8ToUTF16(vendor_name);
    }
  }

  if (serial_number.empty()) {
    if (product.empty()) {
      product = base::ASCIIToUTF16(base::StringPrintf("%04x", product_id));
      if (manufacturer.empty()) {
        manufacturer =
            base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id));
        return l10n_util::GetStringFUTF16(
            IDS_DEVICE_NAME_WITH_UNKNOWN_PRODUCT_UNKNOWN_VENDOR, product,
            manufacturer);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_DEVICE_NAME_WITH_UNKNOWN_PRODUCT_VENDOR, product, manufacturer);
      }
    } else {
      if (always_include_manufacturer) {
        if (manufacturer.empty()) {
          manufacturer =
              base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id));
          return l10n_util::GetStringFUTF16(
              IDS_DEVICE_NAME_WITH_PRODUCT_UNKNOWN_VENDOR, product,
              manufacturer);
        } else {
          return l10n_util::GetStringFUTF16(IDS_DEVICE_NAME_WITH_PRODUCT_VENDOR,
                                            product, manufacturer);
        }
      } else {
        return product;
      }
    }
  } else {
    if (product.empty()) {
      product = base::ASCIIToUTF16(base::StringPrintf("%04x", product_id));
      if (manufacturer.empty()) {
        manufacturer =
            base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id));
        return l10n_util::GetStringFUTF16(
            IDS_DEVICE_NAME_WITH_UNKNOWN_PRODUCT_UNKNOWN_VENDOR_SERIAL, product,
            manufacturer, serial_number);
      } else {
        return l10n_util::GetStringFUTF16(
            IDS_DEVICE_NAME_WITH_UNKNOWN_PRODUCT_VENDOR_SERIAL, product,
            manufacturer, serial_number);
      }
    } else {
      if (always_include_manufacturer) {
        if (manufacturer.empty()) {
          manufacturer =
              base::ASCIIToUTF16(base::StringPrintf("%04x", vendor_id));
          return l10n_util::GetStringFUTF16(
              IDS_DEVICE_NAME_WITH_PRODUCT_UNKNOWN_VENDOR_SERIAL, product,
              manufacturer, serial_number);
        } else {
          return l10n_util::GetStringFUTF16(
              IDS_DEVICE_NAME_WITH_PRODUCT_VENDOR_SERIAL, product, manufacturer,
              serial_number);
        }
      } else {
        return l10n_util::GetStringFUTF16(IDS_DEVICE_NAME_WITH_PRODUCT_SERIAL,
                                          product, serial_number);
      }
    }
  }
}

DevicePermissions* DevicePermissionsManager::GetForExtension(
    const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DevicePermissions* device_permissions = GetInternal(extension_id);
  if (!device_permissions) {
    device_permissions = new DevicePermissions(context_, extension_id);
    extension_id_to_device_permissions_[extension_id] = device_permissions;
  }

  return device_permissions;
}

std::vector<base::string16>
DevicePermissionsManager::GetPermissionMessageStrings(
    const std::string& extension_id) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<base::string16> messages;
  const DevicePermissions* device_permissions = GetInternal(extension_id);
  if (device_permissions) {
    for (const scoped_refptr<DevicePermissionEntry>& entry :
         device_permissions->entries()) {
      messages.push_back(entry->GetPermissionMessageString());
    }
  }
  return messages;
}

void DevicePermissionsManager::AllowUsbDevice(
    const std::string& extension_id,
    const device::mojom::UsbDeviceInfo& device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DevicePermissions* device_permissions = GetForExtension(extension_id);
  auto device_entry = base::MakeRefCounted<DevicePermissionEntry>(device_info);

  if (device_entry->IsPersistent()) {
    for (const auto& entry : device_permissions->entries()) {
      if (entry->vendor_id() == device_entry->vendor_id() &&
          entry->product_id() == device_entry->product_id() &&
          entry->serial_number() == device_entry->serial_number()) {
        return;
      }
    }

    device_permissions->entries_.insert(device_entry);
    SaveDevicePermissionEntry(context_, extension_id, device_entry);
  } else if (!base::Contains(device_permissions->ephemeral_usb_devices_,
                             device_info.guid)) {
    // Non-persistent devices cannot be reliably identified when they are
    // reconnected so such devices are only remembered until disconnect.
    // Register an observer here so that this set doesn't grow undefinitely.
    device_permissions->entries_.insert(device_entry);
    device_permissions->ephemeral_usb_devices_[device_info.guid] = device_entry;

    // Make sure the UsbDeviceManager has been connected to the DeviceService.
    // UsbDeviceManager is responsible for removing the permission entry for
    // an ephemeral USB device. Only do this when an ephemeral device has been
    // added.
    UsbDeviceManager* device_manager = UsbDeviceManager::Get(context_);
    DCHECK(device_manager);
    device_manager->EnsureConnectionWithDeviceManager();
  }
}

void DevicePermissionsManager::AllowHidDevice(
    const std::string& extension_id,
    const device::mojom::HidDeviceInfo& device) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DevicePermissions* device_permissions = GetForExtension(extension_id);

  auto device_entry = base::MakeRefCounted<DevicePermissionEntry>(device);

  if (device_entry->IsPersistent()) {
    for (const auto& entry : device_permissions->entries()) {
      if (entry->vendor_id() == device_entry->vendor_id() &&
          entry->product_id() == device_entry->product_id() &&
          entry->serial_number() == device_entry->serial_number()) {
        return;
      }
    }

    device_permissions->entries_.insert(device_entry);
    SaveDevicePermissionEntry(context_, extension_id, device_entry);
  } else if (!base::Contains(device_permissions->ephemeral_hid_devices_,
                             device.guid)) {
    // Non-persistent devices cannot be reliably identified when they are
    // reconnected so such devices are only remembered until disconnect.
    // Register an observer here so that this set doesn't grow undefinitely.
    device_permissions->entries_.insert(device_entry);
    device_permissions->ephemeral_hid_devices_[device.guid] = device_entry;

    // Make sure the HidDeviceManager is active. HidDeviceManager is
    // responsible for removing the permission entry for an ephemeral hid
    // device. Only do this when an ephemeral device has been added.
    HidDeviceManager* device_manager = HidDeviceManager::Get(context_);
    DCHECK(device_manager);
    device_manager->LazyInitialize();
  }
}

void DevicePermissionsManager::UpdateLastUsed(
    const std::string& extension_id,
    scoped_refptr<DevicePermissionEntry> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  entry->set_last_used(base::Time::Now());
  if (entry->IsPersistent()) {
    UpdateDevicePermissionEntry(context_, extension_id, entry);
  }
}

void DevicePermissionsManager::RemoveEntry(
    const std::string& extension_id,
    scoped_refptr<DevicePermissionEntry> entry) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DevicePermissions* device_permissions = GetInternal(extension_id);
  DCHECK(device_permissions);
  DCHECK(base::Contains(device_permissions->entries_, entry));
  device_permissions->entries_.erase(entry);
  if (entry->IsPersistent()) {
    RemoveDevicePermissionEntry(context_, extension_id, entry);
  } else if (entry->type_ == DevicePermissionEntry::Type::USB) {
    device_permissions->ephemeral_usb_devices_.erase(entry->device_guid_);
  } else if (entry->type_ == DevicePermissionEntry::Type::HID) {
    device_permissions->ephemeral_hid_devices_.erase(entry->device_guid_);
  } else {
    NOTREACHED();
  }
}

void DevicePermissionsManager::Clear(const std::string& extension_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  ClearDevicePermissionEntries(ExtensionPrefs::Get(context_), extension_id);
  DevicePermissions* device_permissions = GetInternal(extension_id);
  if (device_permissions) {
    extension_id_to_device_permissions_.erase(extension_id);
    delete device_permissions;
  }
}

DevicePermissionsManager::DevicePermissionsManager(
    content::BrowserContext* context)
    : context_(context) {}

DevicePermissionsManager::~DevicePermissionsManager() {
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    DevicePermissions* device_permissions = map_entry.second;
    delete device_permissions;
  }
}

DevicePermissions* DevicePermissionsManager::GetInternal(
    const std::string& extension_id) const {
  auto it = extension_id_to_device_permissions_.find(extension_id);
  if (it != extension_id_to_device_permissions_.end()) {
    return it->second;
  }

  return NULL;
}

void DevicePermissionsManager::RemoveEntryByDeviceGUID(
    DevicePermissionEntry::Type type,
    const std::string& guid) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& map_entry : extension_id_to_device_permissions_) {
    // An ephemeral device cannot be identified if it is reconnected and so
    // permission to access it is cleared on disconnect.
    DevicePermissions* device_permissions = map_entry.second;
    auto& devices = (type == DevicePermissionEntry::Type::HID)
                        ? device_permissions->ephemeral_hid_devices_
                        : device_permissions->ephemeral_usb_devices_;
    const auto& device_entry = devices.find(guid);
    if (device_entry != devices.end()) {
      device_permissions->entries_.erase(device_entry->second);
      devices.erase(device_entry);
    }
  }
}

// static
DevicePermissionsManager* DevicePermissionsManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DevicePermissionsManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
DevicePermissionsManagerFactory*
DevicePermissionsManagerFactory::GetInstance() {
  return base::Singleton<DevicePermissionsManagerFactory>::get();
}

DevicePermissionsManagerFactory::DevicePermissionsManagerFactory()
    : BrowserContextKeyedServiceFactory(
          "DevicePermissionsManager",
          BrowserContextDependencyManager::GetInstance()) {
}

DevicePermissionsManagerFactory::~DevicePermissionsManagerFactory() {
}

KeyedService* DevicePermissionsManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevicePermissionsManager(context);
}

BrowserContext* DevicePermissionsManagerFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // Return the original (possibly off-the-record) browser context so that a
  // separate instance of the DevicePermissionsManager is used in incognito
  // mode. The parent class's implemenation returns NULL.
  return context;
}

}  // namespace extensions
