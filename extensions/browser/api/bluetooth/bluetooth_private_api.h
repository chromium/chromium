// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_
#define EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "device/bluetooth/bluetooth_device.h"
#include "extensions/browser/api/bluetooth/bluetooth_extension_function.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/bluetooth_private.h"

namespace device {
class BluetoothAdapter;
}

namespace extensions {

// The profile-keyed service that manages the bluetoothPrivate extension API.
class BluetoothPrivateAPI : public BrowserContextKeyedAPI,
                            public EventRouter::Observer {
 public:
  static BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>*
      GetFactoryInstance();

  explicit BluetoothPrivateAPI(content::BrowserContext* context);
  ~BluetoothPrivateAPI() override;

  // KeyedService implementation.
  void Shutdown() override;

  // EventRouter::Observer implementation.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "BluetoothPrivateAPI"; }
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<BluetoothPrivateAPI>;

  raw_ptr<content::BrowserContext> browser_context_;
};

namespace api {

namespace bluetooth_private {
namespace SetAdapterState {
struct Params;
}  // namespace SetAdapterState
namespace SetPairingResponse {
struct Params;
}  // namespace SetPairingResponse
namespace DisconnectAll {
struct Params;
}  // namespace DisconnectAll
namespace ForgetDevice {
struct Params;
}  // namespace ForgetDevice
namespace SetDiscoveryFilter {
struct Params;
}  // namespace SetDiscoveryFilter
namespace Connect {
struct Params;
}  // namespace Connect
namespace Pair {
struct Params;
}  // namespace Pair
namespace RecordPairing {
struct Params;
}  // namespace RecordPairing
namespace RecordReconnection {
struct Params;
}  // namespace RecordReconnection
namespace RecordDeviceSelection {
struct Params;
}  // namespace RecordDeviceSelection
}  // namespace bluetooth_private

class BluetoothPrivateSetAdapterStateFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.setAdapterState",
                             BLUETOOTHPRIVATE_SETADAPTERSTATE)
  BluetoothPrivateSetAdapterStateFunction();

  BluetoothPrivateSetAdapterStateFunction(
      const BluetoothPrivateSetAdapterStateFunction&) = delete;
  BluetoothPrivateSetAdapterStateFunction& operator=(
      const BluetoothPrivateSetAdapterStateFunction&) = delete;

 private:
  ~BluetoothPrivateSetAdapterStateFunction() override;

  base::OnceClosure CreatePropertySetCallback(const std::string& property_name);
  base::OnceClosure CreatePropertyErrorCallback(
      const std::string& property_name);
  void OnAdapterPropertySet(const std::string& property);
  void OnAdapterPropertyError(const std::string& property);
  void SendError();

  // BluetoothExtensionFunction overrides:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

  // Set of expected adapter properties to be changed.
  std::set<std::string> pending_properties_;

  // Set of adapter properties that were not set successfully.
  std::set<std::string> failed_properties_;

  // Whether or not the function has finished parsing the arguments and queuing
  // up state requests.
  bool parsed_ = false;

  std::optional<bluetooth_private::SetAdapterState::Params> params_;
};

class BluetoothPrivateSetPairingResponseFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.setPairingResponse",
                             BLUETOOTHPRIVATE_SETPAIRINGRESPONSE)
  BluetoothPrivateSetPairingResponseFunction();

  BluetoothPrivateSetPairingResponseFunction(
      const BluetoothPrivateSetPairingResponseFunction&) = delete;
  BluetoothPrivateSetPairingResponseFunction& operator=(
      const BluetoothPrivateSetPairingResponseFunction&) = delete;

  // BluetoothExtensionFunction overrides:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  ~BluetoothPrivateSetPairingResponseFunction() override;

  std::optional<bluetooth_private::SetPairingResponse::Params> params_;
};

class BluetoothPrivateDisconnectAllFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.disconnectAll",
                             BLUETOOTHPRIVATE_DISCONNECTALL)
  BluetoothPrivateDisconnectAllFunction();

  BluetoothPrivateDisconnectAllFunction(
      const BluetoothPrivateDisconnectAllFunction&) = delete;
  BluetoothPrivateDisconnectAllFunction& operator=(
      const BluetoothPrivateDisconnectAllFunction&) = delete;

  // BluetoothExtensionFunction overrides:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  ~BluetoothPrivateDisconnectAllFunction() override;

  void OnSuccessCallback();
  void OnErrorCallback(scoped_refptr<device::BluetoothAdapter> adapter,
                       const std::string& device_address);

  std::optional<bluetooth_private::DisconnectAll::Params> params_;
};

class BluetoothPrivateForgetDeviceFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.forgetDevice",
                             BLUETOOTHPRIVATE_FORGETDEVICE)
  BluetoothPrivateForgetDeviceFunction();

  BluetoothPrivateForgetDeviceFunction(
      const BluetoothPrivateForgetDeviceFunction&) = delete;
  BluetoothPrivateForgetDeviceFunction& operator=(
      const BluetoothPrivateForgetDeviceFunction&) = delete;

  // BluetoothExtensionFunction overrides:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  ~BluetoothPrivateForgetDeviceFunction() override;

  void OnSuccessCallback();
  void OnErrorCallback(scoped_refptr<device::BluetoothAdapter> adapter,
                       const std::string& device_address);

  std::optional<bluetooth_private::ForgetDevice::Params> params_;
};

class BluetoothPrivateSetDiscoveryFilterFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.setDiscoveryFilter",
                             BLUETOOTHPRIVATE_SETDISCOVERYFILTER)
  BluetoothPrivateSetDiscoveryFilterFunction();

  BluetoothPrivateSetDiscoveryFilterFunction(
      const BluetoothPrivateSetDiscoveryFilterFunction&) = delete;
  BluetoothPrivateSetDiscoveryFilterFunction& operator=(
      const BluetoothPrivateSetDiscoveryFilterFunction&) = delete;

 protected:
  ~BluetoothPrivateSetDiscoveryFilterFunction() override;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  void OnSuccessCallback();
  void OnErrorCallback();

  std::optional<bluetooth_private::SetDiscoveryFilter::Params> params_;
};

class BluetoothPrivateConnectFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.connect",
                             BLUETOOTHPRIVATE_CONNECT)
  BluetoothPrivateConnectFunction();

  BluetoothPrivateConnectFunction(const BluetoothPrivateConnectFunction&) =
      delete;
  BluetoothPrivateConnectFunction& operator=(
      const BluetoothPrivateConnectFunction&) = delete;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  ~BluetoothPrivateConnectFunction() override;

  void OnConnect(
      std::optional<device::BluetoothDevice::ConnectErrorCode> error);

  std::optional<bluetooth_private::Connect::Params> params_;
};

class BluetoothPrivatePairFunction : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.pair", BLUETOOTHPRIVATE_PAIR)
  BluetoothPrivatePairFunction();

  BluetoothPrivatePairFunction(const BluetoothPrivatePairFunction&) = delete;
  BluetoothPrivatePairFunction& operator=(const BluetoothPrivatePairFunction&) =
      delete;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  ~BluetoothPrivatePairFunction() override;

  void OnPair(
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  std::optional<bluetooth_private::Pair::Params> params_;
};

class BluetoothPrivateRecordPairingFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.recordPairing",
                             BLUETOOTHPRIVATE_RECORDPAIRING)

  BluetoothPrivateRecordPairingFunction();

  BluetoothPrivateRecordPairingFunction(
      const BluetoothPrivateRecordPairingFunction&) = delete;
  BluetoothPrivateRecordPairingFunction& operator=(
      const BluetoothPrivateRecordPairingFunction&) = delete;

 protected:
  ~BluetoothPrivateRecordPairingFunction() override;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  std::optional<bluetooth_private::RecordPairing::Params> params_;
};

class BluetoothPrivateRecordReconnectionFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.recordReconnection",
                             BLUETOOTHPRIVATE_RECORDRECONNECTION)

  BluetoothPrivateRecordReconnectionFunction();

  BluetoothPrivateRecordReconnectionFunction(
      const BluetoothPrivateRecordReconnectionFunction&) = delete;
  BluetoothPrivateRecordReconnectionFunction& operator=(
      const BluetoothPrivateRecordReconnectionFunction&) = delete;

 protected:
  ~BluetoothPrivateRecordReconnectionFunction() override;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  std::optional<bluetooth_private::RecordReconnection::Params> params_;
};

class BluetoothPrivateRecordDeviceSelectionFunction
    : public BluetoothExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("bluetoothPrivate.recordDeviceSelection",
                             BLUETOOTHPRIVATE_RECORDDEVICESELECTION)

  BluetoothPrivateRecordDeviceSelectionFunction();

  BluetoothPrivateRecordDeviceSelectionFunction(
      const BluetoothPrivateRecordDeviceSelectionFunction&) = delete;
  BluetoothPrivateRecordDeviceSelectionFunction& operator=(
      const BluetoothPrivateRecordDeviceSelectionFunction&) = delete;

 protected:
  ~BluetoothPrivateRecordDeviceSelectionFunction() override;

  // BluetoothExtensionFunction:
  bool CreateParams() override;
  void DoWork(scoped_refptr<device::BluetoothAdapter> adapter) override;

 private:
  std::optional<bluetooth_private::RecordDeviceSelection::Params> params_;
};

}  // namespace api

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_BLUETOOTH_BLUETOOTH_PRIVATE_API_H_
