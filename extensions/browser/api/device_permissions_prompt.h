// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_PROMPT_H_
#define EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_PROMPT_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace device {
class HidDeviceFilter;
}

namespace extensions {

class Extension;

// Platform-independent interface for displaing a UI for choosing devices
// (similar to choosing files).
class DevicePermissionsPrompt {
 public:
  using UsbDevicesCallback =
      base::OnceCallback<void(std::vector<device::mojom::UsbDeviceInfoPtr>)>;
  using HidDevicesCallback =
      base::OnceCallback<void(std::vector<device::mojom::HidDeviceInfoPtr>)>;

  // Context information available to the UI implementation.
  class Prompt : public base::RefCounted<Prompt> {
   public:
    // This class stores the device information displayed in the UI. It should
    // be extended to support particular device types.
    class DeviceInfo {
     public:
      DeviceInfo();
      virtual ~DeviceInfo();

      const std::u16string& name() const { return name_; }
      const std::u16string& serial_number() const { return serial_number_; }
      bool granted() const { return granted_; }
      void set_granted() { granted_ = true; }

     protected:
      std::u16string name_;
      std::u16string serial_number_;

     private:
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
           bool multiple);

    Prompt(const Prompt&) = delete;
    Prompt& operator=(const Prompt&) = delete;

    // Only one observer may be registered at a time.
    virtual void SetObserver(Observer* observer);

    size_t GetDeviceCount() const { return devices_.size(); }
    std::u16string GetDeviceName(size_t index) const;
    std::u16string GetDeviceSerialNumber(size_t index) const;

    // Notifies the DevicePermissionsManager for the current extension that
    // access to the device at the given index is now granted.
    void GrantDevicePermission(size_t index);

    virtual void Dismissed() = 0;

    // Allow the user to select multiple devices.
    bool multiple() const { return multiple_; }

   protected:
    virtual ~Prompt();

    void AddDevice(std::unique_ptr<DeviceInfo> device);

    const Extension* extension() const { return extension_; }
    Observer* observer() const { return observer_; }
    content::BrowserContext* browser_context() const {
      return browser_context_;
    }

    // Subclasses may fill this with a particular subclass of DeviceInfo and may
    // assume that only that instances of that type are stored here.
    std::vector<std::unique_ptr<DeviceInfo>> devices_;

   private:
    friend class base::RefCounted<Prompt>;

    raw_ptr<const extensions::Extension, DanglingUntriaged> extension_ =
        nullptr;
    raw_ptr<Observer, DanglingUntriaged> observer_ = nullptr;
    raw_ptr<content::BrowserContext, DanglingUntriaged> browser_context_ =
        nullptr;
    bool multiple_ = false;
  };

  explicit DevicePermissionsPrompt(content::WebContents* web_contents);

  DevicePermissionsPrompt(const DevicePermissionsPrompt&) = delete;
  DevicePermissionsPrompt& operator=(const DevicePermissionsPrompt&) = delete;

  virtual ~DevicePermissionsPrompt();

  void AskForUsbDevices(const Extension* extension,
                        content::BrowserContext* context,
                        bool multiple,
                        std::vector<device::mojom::UsbDeviceFilterPtr> filters,
                        UsbDevicesCallback callback);

  void AskForHidDevices(const Extension* extension,
                        content::BrowserContext* context,
                        bool multiple,
                        const std::vector<device::HidDeviceFilter>& filters,
                        HidDevicesCallback callback);

  static scoped_refptr<Prompt> CreateHidPromptForTest(
      const Extension* extension,
      bool multiple);
  static scoped_refptr<Prompt> CreateUsbPromptForTest(
      const Extension* extension,
      bool multiple);

  // Allows tests to override how the HidManager interface is bound.
  using HidManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::HidManager> receiver)>;
  static void OverrideHidManagerBinderForTesting(HidManagerBinder binder);

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

#endif  // EXTENSIONS_BROWSER_API_DEVICE_PERMISSIONS_PROMPT_H_
