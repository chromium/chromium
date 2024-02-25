// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/bluetooth.h"

namespace content {
class BrowserContext;
}

namespace device {
class BluetoothAdapter;
}

namespace extensions {

class BluetoothEventRouter;

// The profile-keyed service that manages the bluetooth extension API.
// All methods of this class must be called on the UI thread.
// TODO(rpaquay): Rename this and move to separate file.
class BluetoothAPI : public BrowserContextKeyedAPI,
                     public EventRouter::Observer {
 public:
  // Convenience method to get the BluetoothAPI for a browser context.
  static BluetoothAPI* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<BluetoothAPI>* GetFactoryInstance();

  explicit BluetoothAPI(content::BrowserContext* context);
  ~BluetoothAPI() override;

  BluetoothEventRouter* event_router();

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

 private:
  // BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<BluetoothAPI>;
  static const char* service_name() { return "BluetoothAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  raw_ptr<content::BrowserContext> browser_context_;

  // Created lazily on first access.
  std::unique_ptr<BluetoothEventRouter> event_router_;
};

template <>
void BrowserContextKeyedAPIFactory<BluetoothAPI>::DeclareFactoryDependencies();

namespace api {

class BluetoothGetAdapterStateFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getAdapterState",
                             BLUETOOTH_GETADAPTERSTATE)

 protected:
  ~BluetoothGetAdapterStateFunction() override;

  // BluetoothExtensionFunction:
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;
};

class BluetoothGetDevicesFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevices", BLUETOOTH_GETDEVICES)

  BluetoothGetDevicesFunction();

  BluetoothGetDevicesFunction(const BluetoothGetDevicesFunction&) = delete;
  BluetoothGetDevicesFunction& operator=(const BluetoothGetDevicesFunction&) =
      delete;

 protected:
  ~BluetoothGetDevicesFunction() override;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  std::optional<bluetooth::GetDevices::Params> params_;
};

class BluetoothGetDeviceFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.getDevice", BLUETOOTH_GETDEVICE)

  BluetoothGetDeviceFunction();

  BluetoothGetDeviceFunction(const BluetoothGetDeviceFunction&) = delete;
  BluetoothGetDeviceFunction& operator=(const BluetoothGetDeviceFunction&) =
      delete;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 protected:
  ~BluetoothGetDeviceFunction() override;

 private:
  std::optional<extensions::api::bluetooth::GetDevice::Params> params_;
};

class BluetoothStartDiscoveryFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.startDiscovery",
                             BLUETOOTH_STARTDISCOVERY)

 protected:
  ~BluetoothStartDiscoveryFunction() override {}

  // BluetoothExtensionFunction:
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

class BluetoothStopDiscoveryFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetooth.stopDiscovery", BLUETOOTH_STOPDISCOVERY)

 protected:
  ~BluetoothStopDiscoveryFunction() override {}

  // BluetoothExtensionFunction:
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_API_H_
