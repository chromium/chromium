// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_API_H_
#define EXTENSIONS_BROWSER_API_HID_HID_API_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/hid/hid_connection_resource.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/hid.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace extensions {

class DevicePermissionsPrompt;

class HidGetDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getDevices", HID_GETDEVICES)

  HidGetDevicesFunction();

 private:
  ~HidGetDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnEnumerationComplete(std::unique_ptr<base::ListValue> devices);

  DISALLOW_COPY_AND_ASSIGN(HidGetDevicesFunction);
};

class HidGetUserSelectedDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getUserSelectedDevices",
                             HID_GETUSERSELECTEDDEVICES)

  HidGetUserSelectedDevicesFunction();

 private:
  ~HidGetUserSelectedDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnDevicesChosen(std::vector<device::mojom::HidDeviceInfoPtr> devices);

  std::unique_ptr<DevicePermissionsPrompt> prompt_;

  DISALLOW_COPY_AND_ASSIGN(HidGetUserSelectedDevicesFunction);
};

class HidConnectFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.connect", HID_CONNECT)

  HidConnectFunction();

 private:
  ~HidConnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnConnectComplete(
      mojo::PendingRemote<device::mojom::HidConnection> connection);

  ApiResourceManager<HidConnectionResource>* connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(HidConnectFunction);
};

class HidDisconnectFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.disconnect", HID_DISCONNECT)

  HidDisconnectFunction();

 private:
  ~HidDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  DISALLOW_COPY_AND_ASSIGN(HidDisconnectFunction);
};

// Base class for extension functions that start some asynchronous work after
// looking up a HidConnection.
class HidConnectionIoFunction : public ExtensionFunction {
 public:
  HidConnectionIoFunction();

 protected:
  ~HidConnectionIoFunction() override;

  // Returns true if params were successfully read from |args_|.
  virtual bool ReadParameters() = 0;
  virtual void StartWork(device::mojom::HidConnection* connection) = 0;

  void set_connection_id(int connection_id) { connection_id_ = connection_id; }

 private:
  // ExtensionFunction:
  ResponseAction Run() override;

  int connection_id_;
};

class HidReceiveFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receive", HID_RECEIVE)

  HidReceiveFunction();

 private:
  ~HidReceiveFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success,
                  uint8_t report_id,
                  const base::Optional<std::vector<uint8_t>>& buffer);

  std::unique_ptr<api::hid::Receive::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFunction);
};

class HidSendFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.send", HID_SEND)

  HidSendFunction();

 private:
  ~HidSendFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success);

  std::unique_ptr<api::hid::Send::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFunction);
};

class HidReceiveFeatureReportFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receiveFeatureReport",
                             HID_RECEIVEFEATUREREPORT)

  HidReceiveFeatureReportFunction();

 private:
  ~HidReceiveFeatureReportFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success,
                  const base::Optional<std::vector<uint8_t>>& buffer);

  std::unique_ptr<api::hid::ReceiveFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidReceiveFeatureReportFunction);
};

class HidSendFeatureReportFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.sendFeatureReport", HID_SENDFEATUREREPORT)

  HidSendFeatureReportFunction();

 private:
  ~HidSendFeatureReportFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success);

  std::unique_ptr<api::hid::SendFeatureReport::Params> parameters_;

  DISALLOW_COPY_AND_ASSIGN(HidSendFeatureReportFunction);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_API_H_
