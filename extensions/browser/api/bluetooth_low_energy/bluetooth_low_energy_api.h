// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_context.h"
#include "device/bluetooth/bluetooth_advertisement.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_api_advertisement.h"
#include "extensions/browser/api/bluetooth_low_energy/bluetooth_low_energy_event_router.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {
class BluetoothApiAdvertisement;
class BluetoothLowEnergyEventRouter;

namespace api {
namespace bluetooth_low_energy {
namespace CreateService {
struct Params;
}
namespace CreateCharacteristic {
struct Params;
}
namespace CreateDescriptor {
struct Params;
}
namespace RegisterService {
struct Params;
}
namespace UnregisterService {
struct Params;
}
namespace NotifyCharacteristicValueChanged {
struct Params;
}
namespace RemoveService {
struct Params;
}
namespace SendRequestResponse {
struct Params;
}
}  // namespace bluetooth_low_energy
}  // namespace api

// The profile-keyed service that manages the bluetoothLowEnergy extension API.
class BluetoothLowEnergyAPI : public BrowserContextKeyedAPI {
 public:
  static BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>*
  GetFactoryInstance();

  // Convenience method to get the BluetoothLowEnergy API for a browser context.
  static BluetoothLowEnergyAPI* Get(content::BrowserContext* context);

  explicit BluetoothLowEnergyAPI(content::BrowserContext* context);
  ~BluetoothLowEnergyAPI() override;

  // KeyedService implementation..
  void Shutdown() override;

  BluetoothLowEnergyEventRouter* event_router() const {
    return event_router_.get();
  }

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothLowEnergyAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothLowEnergyAPI>;

  std::unique_ptr<BluetoothLowEnergyEventRouter> event_router_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAPI);
};

namespace api {

// Base class for bluetoothLowEnergy API functions. This class handles some of
// the common logic involved in all API functions, such as checking for
// platform support and returning the correct error.
class BluetoothLowEnergyExtensionFunction : public ExtensionFunction {
 public:
  BluetoothLowEnergyExtensionFunction();

 protected:
  ~BluetoothLowEnergyExtensionFunction() override;

  // ExtensionFunction override.
  ResponseAction Run() override;

  // Implemented by individual bluetoothLowEnergy extension functions to perform
  // the body of the function. This invoked asynchonously after Run after
  // the BluetoothLowEnergyEventRouter has obtained a handle on the
  // BluetoothAdapter.
  virtual void DoWork() = 0;

  // Subclasses to implement this method to set and validate its type-specific
  // params. This method should return true if the params is valid and
  // false otherwise. This method is called before DoWork() to allow early exit
  // in the case of invalid params.
  virtual bool ParseParams() = 0;

  BluetoothLowEnergyEventRouter* event_router_;

 private:
  // Internal method to do common setup before actual DoWork is called.
  void PreDoWork();

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyExtensionFunction);
};

// Base class for bluetoothLowEnergy API peripheral mode functions. This class
// handles some of the common logic involved in all API peripheral mode
// functions, such as checking for peripheral permissions and returning the
// correct error.
class BLEPeripheralExtensionFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  BLEPeripheralExtensionFunction();

 protected:
  ~BLEPeripheralExtensionFunction() override;

  // ExtensionFunction override.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(BLEPeripheralExtensionFunction);
};

class BluetoothLowEnergyConnectFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.connect",
                             BLUETOOTHLOWENERGY_CONNECT)

  BluetoothLowEnergyConnectFunction();

 protected:
  ~BluetoothLowEnergyConnectFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::Connect::Params> params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Connect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyDisconnectFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.disconnect",
                             BLUETOOTHLOWENERGY_DISCONNECT)

  BluetoothLowEnergyDisconnectFunction();

 protected:
  ~BluetoothLowEnergyDisconnectFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::Disconnect::Params> params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::Disconnect.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyGetServiceFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getService",
                             BLUETOOTHLOWENERGY_GETSERVICE)

  BluetoothLowEnergyGetServiceFunction();

 protected:
  ~BluetoothLowEnergyGetServiceFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetService::Params> params_;
};

class BluetoothLowEnergyGetServicesFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getServices",
                             BLUETOOTHLOWENERGY_GETSERVICES)

  BluetoothLowEnergyGetServicesFunction();

 protected:
  ~BluetoothLowEnergyGetServicesFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetServices::Params> params_;
};

class BluetoothLowEnergyGetCharacteristicFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristic",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTIC)

  BluetoothLowEnergyGetCharacteristicFunction();

 protected:
  ~BluetoothLowEnergyGetCharacteristicFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetCharacteristic::Params> params_;
};

class BluetoothLowEnergyGetCharacteristicsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getCharacteristics",
                             BLUETOOTHLOWENERGY_GETCHARACTERISTICS)

  BluetoothLowEnergyGetCharacteristicsFunction();

 protected:
  ~BluetoothLowEnergyGetCharacteristicsFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetCharacteristics::Params> params_;
};

class BluetoothLowEnergyGetIncludedServicesFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getIncludedServices",
                             BLUETOOTHLOWENERGY_GETINCLUDEDSERVICES)

  BluetoothLowEnergyGetIncludedServicesFunction();

 protected:
  ~BluetoothLowEnergyGetIncludedServicesFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetIncludedServices::Params> params_;
};

class BluetoothLowEnergyGetDescriptorFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptor",
                             BLUETOOTHLOWENERGY_GETDESCRIPTOR)

  BluetoothLowEnergyGetDescriptorFunction();

 protected:
  ~BluetoothLowEnergyGetDescriptorFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetDescriptor::Params> params_;
};

class BluetoothLowEnergyGetDescriptorsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.getDescriptors",
                             BLUETOOTHLOWENERGY_GETDESCRIPTORS)

  BluetoothLowEnergyGetDescriptorsFunction();

 protected:
  ~BluetoothLowEnergyGetDescriptorsFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::GetDescriptors::Params> params_;
};

class BluetoothLowEnergyReadCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readCharacteristicValue",
                             BLUETOOTHLOWENERGY_READCHARACTERISTICVALUE)

  BluetoothLowEnergyReadCharacteristicValueFunction();

 protected:
  ~BluetoothLowEnergyReadCharacteristicValueFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::ReadCharacteristicValue::Params>
      params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteCharacteristicValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeCharacteristicValue",
                             BLUETOOTHLOWENERGY_WRITECHARACTERISTICVALUE)

  BluetoothLowEnergyWriteCharacteristicValueFunction();

 protected:
  ~BluetoothLowEnergyWriteCharacteristicValueFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::WriteCharacteristicValue::Params>
      params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteCharacteristicValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested characteristic.
  std::string instance_id_;
};

class BluetoothLowEnergyStartCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.startCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STARTCHARACTERISTICNOTIFICATIONS)

  BluetoothLowEnergyStartCharacteristicNotificationsFunction();

 protected:
  ~BluetoothLowEnergyStartCharacteristicNotificationsFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<
      bluetooth_low_energy::StartCharacteristicNotifications::Params>
      params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StartCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyStopCharacteristicNotificationsFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.stopCharacteristicNotifications",
      BLUETOOTHLOWENERGY_STOPCHARACTERISTICNOTIFICATIONS)

  BluetoothLowEnergyStopCharacteristicNotificationsFunction();

 protected:
  ~BluetoothLowEnergyStopCharacteristicNotificationsFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::StopCharacteristicNotifications::Params>
      params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::StopCharacteristicNotifications.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyReadDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.readDescriptorValue",
                             BLUETOOTHLOWENERGY_READDESCRIPTORVALUE)

  BluetoothLowEnergyReadDescriptorValueFunction();

 protected:
  ~BluetoothLowEnergyReadDescriptorValueFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::ReadDescriptorValue::Params> params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::ReadDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyWriteDescriptorValueFunction
    : public BluetoothLowEnergyExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.writeDescriptorValue",
                             BLUETOOTHLOWENERGY_WRITEDESCRIPTORVALUE)

  BluetoothLowEnergyWriteDescriptorValueFunction();

 protected:
  ~BluetoothLowEnergyWriteDescriptorValueFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::WriteDescriptorValue::Params> params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::WriteDescriptorValue.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);

  // The instance ID of the requested descriptor.
  std::string instance_id_;
};

class BluetoothLowEnergyAdvertisementFunction
    : public BLEPeripheralExtensionFunction {
 public:
  BluetoothLowEnergyAdvertisementFunction();

 protected:
  ~BluetoothLowEnergyAdvertisementFunction() override;

  // Takes ownership.
  int AddAdvertisement(BluetoothApiAdvertisement* advertisement);
  BluetoothApiAdvertisement* GetAdvertisement(int advertisement_id);
  void RemoveAdvertisement(int advertisement_id);
  const std::unordered_set<int>* GetAdvertisementIds();

  // ExtensionFunction override.
  ResponseAction Run() override;

 private:
  void Initialize();

  ApiResourceManager<BluetoothApiAdvertisement>* advertisements_manager_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothLowEnergyAdvertisementFunction);
};

class BluetoothLowEnergyRegisterAdvertisementFunction
    : public BluetoothLowEnergyAdvertisementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.registerAdvertisement",
                             BLUETOOTHLOWENERGY_REGISTERADVERTISEMENT)

  BluetoothLowEnergyRegisterAdvertisementFunction();

 protected:
  ~BluetoothLowEnergyRegisterAdvertisementFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::RegisterAdvertisement::Params> params_;

 private:
  void SuccessCallback(scoped_refptr<device::BluetoothAdvertisement>);
  void ErrorCallback(device::BluetoothAdvertisement::ErrorCode status);
};

class BluetoothLowEnergyUnregisterAdvertisementFunction
    : public BluetoothLowEnergyAdvertisementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.unregisterAdvertisement",
                             BLUETOOTHLOWENERGY_UNREGISTERADVERTISEMENT)

  BluetoothLowEnergyUnregisterAdvertisementFunction();

 protected:
  ~BluetoothLowEnergyUnregisterAdvertisementFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::UnregisterAdvertisement::Params>
      params_;

 private:
  void SuccessCallback(int advertisement_id);
  void ErrorCallback(int advertisement_id,
                     device::BluetoothAdvertisement::ErrorCode status);
};

class BluetoothLowEnergyResetAdvertisingFunction
    : public BluetoothLowEnergyAdvertisementFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.resetAdvertising",
                             BLUETOOTHLOWENERGY_RESETADVERTISING)

  BluetoothLowEnergyResetAdvertisingFunction();

 protected:
  ~BluetoothLowEnergyResetAdvertisingFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

 private:
  void SuccessCallback();
  void ErrorCallback(device::BluetoothAdvertisement::ErrorCode status);
};

class BluetoothLowEnergySetAdvertisingIntervalFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.setAdvertisingInterval",
                             BLUETOOTHLOWENERGY_SETADVERTISINGINTERVAL)

  BluetoothLowEnergySetAdvertisingIntervalFunction();

 protected:
  ~BluetoothLowEnergySetAdvertisingIntervalFunction() override;

  // BluetoothLowEnergyExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::SetAdvertisingInterval::Params> params_;

 private:
  void SuccessCallback();
  void ErrorCallback(device::BluetoothAdvertisement::ErrorCode status);
};

class BluetoothLowEnergyCreateServiceFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createService",
                             BLUETOOTHLOWENERGY_CREATESERVICE)

  BluetoothLowEnergyCreateServiceFunction();

 protected:
  ~BluetoothLowEnergyCreateServiceFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  // Causes link error on Windows. API will never be on Windows, so #ifdefing.
#if !defined(OS_WIN)
  std::unique_ptr<bluetooth_low_energy::CreateService::Params> params_;
#endif
};

class BluetoothLowEnergyCreateCharacteristicFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createCharacteristic",
                             BLUETOOTHLOWENERGY_CREATECHARACTERISTIC)

  BluetoothLowEnergyCreateCharacteristicFunction();

 protected:
  ~BluetoothLowEnergyCreateCharacteristicFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::CreateCharacteristic::Params> params_;
};

class BluetoothLowEnergyNotifyCharacteristicValueChangedFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "bluetoothLowEnergy.notifyCharacteristicValueChanged",
      BLUETOOTHLOWENERGY_NOTIFYCHARACTERISTICVALUECHANGED)

  BluetoothLowEnergyNotifyCharacteristicValueChangedFunction();

 protected:
  ~BluetoothLowEnergyNotifyCharacteristicValueChangedFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<
      bluetooth_low_energy::NotifyCharacteristicValueChanged::Params>
      params_;
};

class BluetoothLowEnergyCreateDescriptorFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.createDescriptor",
                             BLUETOOTHLOWENERGY_CREATEDESCRIPTOR)

  BluetoothLowEnergyCreateDescriptorFunction();

 protected:
  ~BluetoothLowEnergyCreateDescriptorFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::CreateDescriptor::Params> params_;
};

class BluetoothLowEnergyRegisterServiceFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.registerService",
                             BLUETOOTHLOWENERGY_REGISTERSERVICE)

  BluetoothLowEnergyRegisterServiceFunction();

 protected:
  ~BluetoothLowEnergyRegisterServiceFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::RegisterService::Params> params_;

 private:
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyUnregisterServiceFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.unregisterService",
                             BLUETOOTHLOWENERGY_UNREGISTERSERVICE)

  BluetoothLowEnergyUnregisterServiceFunction();

 protected:
  ~BluetoothLowEnergyUnregisterServiceFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::UnregisterService::Params> params_;

 private:
  // Success and error callbacks, called by
  // BluetoothLowEnergyEventRouter::RegisterService.
  void SuccessCallback();
  void ErrorCallback(BluetoothLowEnergyEventRouter::Status status);
};

class BluetoothLowEnergyRemoveServiceFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.removeService",
                             BLUETOOTHLOWENERGY_REMOVESERVICE)

  BluetoothLowEnergyRemoveServiceFunction();

 protected:
  ~BluetoothLowEnergyRemoveServiceFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::RemoveService::Params> params_;
};

class BluetoothLowEnergySendRequestResponseFunction
    : public BLEPeripheralExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothLowEnergy.sendRequestResponse",
                             BLUETOOTHLOWENERGY_SENDREQUESTRESPONSE)

  BluetoothLowEnergySendRequestResponseFunction();

 protected:
  ~BluetoothLowEnergySendRequestResponseFunction() override;

  // BluetoothLowEnergyPeripheralExtensionFunction override.
  void DoWork() override;
  bool ParseParams() override;

  std::unique_ptr<bluetooth_low_energy::SendRequestResponse::Params> params_;
};

}  // namespace api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_LOW_ENERGY_BLUETOOTH_LOW_ENERGY_API_H_
