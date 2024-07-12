// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/hid/hid_api.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/hid.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/cpp/hid/hid_device_filter.h"

namespace hid = extensions::api::hid;

using device::HidDeviceFilter;

namespace {

const char kErrorPermissionDenied[] = "Permission to access device was denied.";
const char kErrorInvalidDeviceId[] = "Invalid HID device ID.";
const char kErrorFailedToOpenDevice[] = "Failed to open HID device.";
const char kErrorConnectionNotFound[] = "Connection not established.";
const char kErrorTransfer[] = "Transfer failed.";

base::Value::Dict PopulateHidConnection(int connection_id) {
  hid::HidConnectInfo connection_value;
  connection_value.connection_id = connection_id;
  return connection_value.ToValue();
}

void ConvertHidDeviceFilter(const hid::DeviceFilter& input,
                            HidDeviceFilter* output) {
  if (input.vendor_id) {
    output->SetVendorId(*input.vendor_id);
  }
  if (input.product_id) {
    output->SetProductId(*input.product_id);
  }
  if (input.usage_page) {
    output->SetUsagePage(*input.usage_page);
  }
  if (input.usage) {
    output->SetUsage(*input.usage);
  }
}

}  // namespace

namespace extensions {

HidGetDevicesFunction::HidGetDevicesFunction() = default;

HidGetDevicesFunction::~HidGetDevicesFunction() = default;

ExtensionFunction::ResponseAction HidGetDevicesFunction::Run() {
  std::optional<api::hid::GetDevices::Params> parameters =
      hid::GetDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  CHECK(device_manager);

  std::vector<HidDeviceFilter> filters;
  if (parameters->options.filters) {
    filters.resize(parameters->options.filters->size());
    for (size_t i = 0; i < parameters->options.filters->size(); ++i) {
      ConvertHidDeviceFilter(parameters->options.filters->at(i), &filters[i]);
    }
  }
  if (parameters->options.vendor_id) {
    HidDeviceFilter legacy_filter;
    legacy_filter.SetVendorId(*parameters->options.vendor_id);
    if (parameters->options.product_id) {
      legacy_filter.SetProductId(*parameters->options.product_id);
    }
    filters.push_back(legacy_filter);
  }

  device_manager->GetApiDevices(
      extension(), filters,
      base::BindOnce(&HidGetDevicesFunction::OnEnumerationComplete, this));
  return RespondLater();
}

void HidGetDevicesFunction::OnEnumerationComplete(base::Value::List devices) {
  Respond(WithArguments(std::move(devices)));
}

HidConnectFunction::HidConnectFunction() : connection_manager_(nullptr) {
}

HidConnectFunction::~HidConnectFunction() = default;

ExtensionFunction::ResponseAction HidConnectFunction::Run() {
  std::optional<api::hid::Connect::Params> parameters =
      hid::Connect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  HidDeviceManager* device_manager = HidDeviceManager::Get(browser_context());
  CHECK(device_manager);

  connection_manager_ =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager_);

  const device::mojom::HidDeviceInfo* device_info =
      device_manager->GetDeviceInfo(parameters->device_id);
  if (!device_info) {
    return RespondNow(Error(kErrorInvalidDeviceId));
  }

  if (!device_manager->HasPermission(extension(), *device_info, true)) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

  device_manager->Connect(
      device_info->guid,
      base::BindOnce(&HidConnectFunction::OnConnectComplete, this));
  return RespondLater();
}

void HidConnectFunction::OnConnectComplete(
    mojo::PendingRemote<device::mojom::HidConnection> connection) {
  if (!connection) {
    Respond(Error(kErrorFailedToOpenDevice));
    return;
  }

  DCHECK(connection_manager_);
  int connection_id = connection_manager_->Add(
      new HidConnectionResource(extension_id(), std::move(connection)));
  Respond(WithArguments(PopulateHidConnection(connection_id)));
}

HidDisconnectFunction::HidDisconnectFunction() = default;

HidDisconnectFunction::~HidDisconnectFunction() = default;

ExtensionFunction::ResponseAction HidDisconnectFunction::Run() {
  std::optional<api::hid::Disconnect::Params> parameters =
      hid::Disconnect::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager);

  int connection_id = parameters->connection_id;
  HidConnectionResource* resource =
      connection_manager->Get(extension_id(), connection_id);
  if (!resource) {
    return RespondNow(Error(kErrorConnectionNotFound));
  }

  connection_manager->Remove(extension_id(), connection_id);
  return RespondNow(NoArguments());
}

HidConnectionIoFunction::HidConnectionIoFunction() {
}

HidConnectionIoFunction::~HidConnectionIoFunction() {
}

ExtensionFunction::ResponseAction HidConnectionIoFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(ReadParameters());

  ApiResourceManager<HidConnectionResource>* connection_manager =
      ApiResourceManager<HidConnectionResource>::Get(browser_context());
  CHECK(connection_manager);

  HidConnectionResource* resource =
      connection_manager->Get(extension_id(), connection_id_);
  if (!resource) {
    return RespondNow(Error(kErrorConnectionNotFound));
  }

  StartWork(resource->connection());
  return RespondLater();
}

HidReceiveFunction::HidReceiveFunction() = default;

HidReceiveFunction::~HidReceiveFunction() = default;

bool HidReceiveFunction::ReadParameters() {
  parameters_ = hid::Receive::Params::Create(args());
  if (!parameters_)
    return false;
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidReceiveFunction::StartWork(device::mojom::HidConnection* connection) {
  connection->Read(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&HidReceiveFunction::OnFinished, this), false, 0,
      std::nullopt));
}

void HidReceiveFunction::OnFinished(
    bool success,
    uint8_t report_id,
    const std::optional<std::vector<uint8_t>>& buffer) {
  if (success) {
    DCHECK(buffer);
    Respond(WithArguments(report_id, base::Value(*buffer)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFunction::HidSendFunction() = default;

HidSendFunction::~HidSendFunction() = default;

bool HidSendFunction::ReadParameters() {
  parameters_ = hid::Send::Params::Create(args());
  if (!parameters_)
    return false;
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidSendFunction::StartWork(device::mojom::HidConnection* connection) {
  connection->Write(
      static_cast<uint8_t>(parameters_->report_id), parameters_->data,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&HidSendFunction::OnFinished, this), false));
}

void HidSendFunction::OnFinished(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidReceiveFeatureReportFunction::HidReceiveFeatureReportFunction() = default;

HidReceiveFeatureReportFunction::~HidReceiveFeatureReportFunction() = default;

bool HidReceiveFeatureReportFunction::ReadParameters() {
  parameters_ = hid::ReceiveFeatureReport::Params::Create(args());
  if (!parameters_)
    return false;
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidReceiveFeatureReportFunction::StartWork(
    device::mojom::HidConnection* connection) {
  connection->GetFeatureReport(
      static_cast<uint8_t>(parameters_->report_id),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&HidReceiveFeatureReportFunction::OnFinished, this),
          false, std::nullopt));
}

void HidReceiveFeatureReportFunction::OnFinished(
    bool success,
    const std::optional<std::vector<uint8_t>>& buffer) {
  if (success) {
    DCHECK(buffer);
    Respond(WithArguments(base::Value(*buffer)));
  } else {
    Respond(Error(kErrorTransfer));
  }
}

HidSendFeatureReportFunction::HidSendFeatureReportFunction() = default;

HidSendFeatureReportFunction::~HidSendFeatureReportFunction() = default;

bool HidSendFeatureReportFunction::ReadParameters() {
  parameters_ = hid::SendFeatureReport::Params::Create(args());
  if (!parameters_)
    return false;
  set_connection_id(parameters_->connection_id);
  return true;
}

void HidSendFeatureReportFunction::StartWork(
    device::mojom::HidConnection* connection) {
  connection->SendFeatureReport(
      static_cast<uint8_t>(parameters_->report_id), parameters_->data,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&HidSendFeatureReportFunction::OnFinished, this),
          false));
}

void HidSendFeatureReportFunction::OnFinished(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorTransfer));
  }
}

}  // namespace extensions
