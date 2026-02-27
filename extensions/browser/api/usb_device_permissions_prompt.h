// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_USB_DEVICE_PERMISSIONS_PROMPT_H_
#define EXTENSIONS_BROWSER_API_USB_DEVICE_PERMISSIONS_PROMPT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/usb/usb_device_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class Extension;

// Platform-independent interface for displaying a UI for choosing devices
// (similar to choosing files).
class UsbDevicePermissionsPrompt {
 public:
  using UsbDevicesCallback =
      base::OnceCallback<void(std::vector<device::mojom::UsbDeviceInfoPtr>)>;

  // Context information available to the UI implementation.
  class Prompt : public base::RefCounted<Prompt>,
                 public UsbDeviceManager::Observer {
   public:
    // This class stores the device information displayed in the UI. It should
    // be extended to support particular device types.
    class DeviceInfo {
     public:
      explicit DeviceInfo(device::mojom::UsbDeviceInfoPtr device);
      ~DeviceInfo();

      const std::u16string& name() const { return name_; }
      const std::u16string& serial_number() const { return serial_number_; }
      bool granted() const { return granted_; }
      void set_granted() { granted_ = true; }
      device::mojom::UsbDeviceInfoPtr& device() { return device_; }

     protected:
      device::mojom::UsbDeviceInfoPtr device_;
      std::u16string name_;
      std::u16string serial_number_;
      bool granted_ = false;
    };

    // Since the set of devices can change while the UI is visible an
    // implementation should register an observer.
    class Observer {
     public:
      // Must be called after OnDeviceAdded() has been called for the final time
      // to create the initial set of options.
      virtual void OnDevicesInitialized() = 0;
      virtual void OnDeviceAdded(size_t index,
                                 const std::u16string& device_name) = 0;
      virtual void OnDeviceRemoved(size_t index,
                                   const std::u16string& device_name) = 0;

     protected:
      virtual ~Observer();
    };

    Prompt(const Extension* extension,
           content::BrowserContext* context,
           bool multiple,
           std::vector<device::mojom::UsbDeviceFilterPtr> filters,
           UsbDevicesCallback callback);

    Prompt(const Prompt&) = delete;
    Prompt& operator=(const Prompt&) = delete;

    // Only one observer may be registered at a time.
    void SetObserver(Observer* observer);

    // UsbDeviceManager::Observer:
    void OnDeviceAdded(const device::mojom::UsbDeviceInfo& device) override;
    void OnDeviceRemoved(const device::mojom::UsbDeviceInfo& device) override;

    size_t GetDeviceCount() const { return devices_.size(); }
    std::u16string GetDeviceName(size_t index) const;
    std::u16string GetDeviceSerialNumber(size_t index) const;

    // Notifies the DevicePermissionsManager for the current extension that
    // access to the device at the given index is now granted.
    void GrantDevicePermission(size_t index);

    void Dismissed();

    // Allow the user to select multiple devices.
    bool multiple() const { return multiple_; }

   private:
    friend class base::RefCounted<Prompt>;

    ~Prompt() override;

    void AddDevice(std::unique_ptr<DeviceInfo> device);

    void OnDevicesEnumerated(
        std::vector<device::mojom::UsbDeviceInfoPtr> devices);
    void MaybeAddDevice(const device::mojom::UsbDeviceInfo& device,
                        bool initial_enumeration);
    void AddCheckedDevice(std::unique_ptr<DeviceInfo> device_info,
                          bool initial_enumeration,
                          bool allowed);

    std::vector<std::unique_ptr<DeviceInfo>> devices_;

    raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ =
        nullptr;
    raw_ptr<Observer, DanglingUntriaged> observer_ = nullptr;
    raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_ =
        nullptr;
    bool multiple_ = false;

    std::vector<device::mojom::UsbDeviceFilterPtr> filters_;
    size_t remaining_initial_devices_ = 0;
    UsbDevicesCallback callback_;
    base::ScopedObservation<UsbDeviceManager, UsbDeviceManager::Observer>
        manager_observation_{this};
  };

  explicit UsbDevicePermissionsPrompt(content::WebContents* web_contents);

  UsbDevicePermissionsPrompt(const UsbDevicePermissionsPrompt&) = delete;
  UsbDevicePermissionsPrompt& operator=(const UsbDevicePermissionsPrompt&) =
      delete;

  virtual ~UsbDevicePermissionsPrompt();

  void AskForDevices(const Extension* extension,
                     content::BrowserContext* context,
                     bool multiple,
                     std::vector<device::mojom::UsbDeviceFilterPtr> filters,
                     UsbDevicesCallback callback);

  static scoped_refptr<Prompt> CreateForTest(const Extension* extension,
                                             bool multiple);

 protected:
  virtual void ShowDialog() = 0;

  content::WebContents* web_contents() { return web_contents_; }
  scoped_refptr<Prompt> prompt() { return prompt_; }

 private:
  // Parent web contents of the device permissions UI dialog.
  raw_ptr<content::WebContents> web_contents_;

  // Parameters available to the UI implementation.
  scoped_refptr<Prompt> prompt_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_USB_DEVICE_PERMISSIONS_PROMPT_H_
