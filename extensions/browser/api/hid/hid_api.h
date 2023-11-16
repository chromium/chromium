// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_HID_HID_API_H_
#define EXTENSIONS_BROWSER_API_HID_HID_API_H_

#include <stddef.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/hid/hid_connection_resource.h"
#include "extensions/browser/api/hid/hid_device_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/api/hid.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace extensions {

class HidGetDevicesFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.getDevices", HID_GETDEVICES)

  HidGetDevicesFunction();

  HidGetDevicesFunction(const HidGetDevicesFunction&) = delete;
  HidGetDevicesFunction& operator=(const HidGetDevicesFunction&) = delete;

 private:
  ~HidGetDevicesFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnEnumerationComplete(base::Value::List devices);
};

class HidConnectFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.connect", HID_CONNECT)

  HidConnectFunction();

  HidConnectFunction(const HidConnectFunction&) = delete;
  HidConnectFunction& operator=(const HidConnectFunction&) = delete;

 private:
  ~HidConnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

  void OnConnectComplete(
      mojo::PendingRemote<device::mojom::HidConnection> connection);

  raw_ptr<ApiResourceManager<HidConnectionResource>> connection_manager_;
};

class HidDisconnectFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.disconnect", HID_DISCONNECT)

  HidDisconnectFunction();

  HidDisconnectFunction(const HidDisconnectFunction&) = delete;
  HidDisconnectFunction& operator=(const HidDisconnectFunction&) = delete;

 private:
  ~HidDisconnectFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

// Base class for extension functions that start some asynchronous work after
// looking up a HidConnection.
class HidConnectionIoFunction : public ExtensionFunction {
 public:
  HidConnectionIoFunction();

 protected:
  ~HidConnectionIoFunction() override;

  // Returns true if params were successfully read from |args()|.
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

  HidReceiveFunction(const HidReceiveFunction&) = delete;
  HidReceiveFunction& operator=(const HidReceiveFunction&) = delete;

 private:
  ~HidReceiveFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success,
                  uint8_t report_id,
                  const std::optional<std::vector<uint8_t>>& buffer);

  std::optional<api::hid::Receive::Params> parameters_;
};

class HidSendFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.send", HID_SEND)

  HidSendFunction();

  HidSendFunction(const HidSendFunction&) = delete;
  HidSendFunction& operator=(const HidSendFunction&) = delete;

 private:
  ~HidSendFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success);

  std::optional<api::hid::Send::Params> parameters_;
};

class HidReceiveFeatureReportFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.receiveFeatureReport",
                             HID_RECEIVEFEATUREREPORT)

  HidReceiveFeatureReportFunction();

  HidReceiveFeatureReportFunction(const HidReceiveFeatureReportFunction&) =
      delete;
  HidReceiveFeatureReportFunction& operator=(
      const HidReceiveFeatureReportFunction&) = delete;

 private:
  ~HidReceiveFeatureReportFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success,
                  const std::optional<std::vector<uint8_t>>& buffer);

  std::optional<api::hid::ReceiveFeatureReport::Params> parameters_;
};

class HidSendFeatureReportFunction : public HidConnectionIoFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("hid.sendFeatureReport", HID_SENDFEATUREREPORT)

  HidSendFeatureReportFunction();

  HidSendFeatureReportFunction(const HidSendFeatureReportFunction&) = delete;
  HidSendFeatureReportFunction& operator=(const HidSendFeatureReportFunction&) =
      delete;

 private:
  ~HidSendFeatureReportFunction() override;

  // HidConnectionIoFunction:
  bool ReadParameters() override;
  void StartWork(device::mojom::HidConnection* connection) override;

  void OnFinished(bool success);

  std::optional<api::hid::SendFeatureReport::Params> parameters_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_HID_HID_API_H_
