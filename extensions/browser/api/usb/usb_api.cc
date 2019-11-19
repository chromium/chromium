// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/usb/usb_api.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/usb/usb_device_resource.h"
#include "extensions/browser/extension_function_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

namespace usb = extensions::api::usb;
namespace BulkTransfer = usb::BulkTransfer;
namespace ClaimInterface = usb::ClaimInterface;
namespace CloseDevice = usb::CloseDevice;
namespace ControlTransfer = usb::ControlTransfer;
namespace FindDevices = usb::FindDevices;
namespace GetConfigurations = usb::GetConfigurations;
namespace GetDevices = usb::GetDevices;
namespace GetUserSelectedDevices = usb::GetUserSelectedDevices;
namespace InterruptTransfer = usb::InterruptTransfer;
namespace IsochronousTransfer = usb::IsochronousTransfer;
namespace SetConfiguration = usb::SetConfiguration;
namespace GetConfiguration = usb::GetConfiguration;
namespace ListInterfaces = usb::ListInterfaces;
namespace OpenDevice = usb::OpenDevice;
namespace ReleaseInterface = usb::ReleaseInterface;
namespace RequestAccess = usb::RequestAccess;
namespace ResetDevice = usb::ResetDevice;
namespace SetInterfaceAlternateSetting = usb::SetInterfaceAlternateSetting;

using content::BrowserThread;
using device::mojom::UsbControlTransferParams;
using device::mojom::UsbControlTransferRecipient;
using device::mojom::UsbControlTransferType;
using device::mojom::UsbDeviceFilterPtr;
using device::mojom::UsbIsochronousPacketPtr;
using device::mojom::UsbSynchronizationType;
using device::mojom::UsbTransferDirection;
using device::mojom::UsbTransferStatus;
using device::mojom::UsbTransferType;
using device::mojom::UsbUsageType;
using std::string;
using std::vector;
using usb::ConfigDescriptor;
using usb::ConnectionHandle;
using usb::ControlTransferInfo;
using usb::Device;
using usb::Direction;
using usb::EndpointDescriptor;
using usb::GenericTransferInfo;
using usb::InterfaceDescriptor;
using usb::IsochronousTransferInfo;
using usb::Recipient;
using usb::RequestType;
using usb::SynchronizationType;
using usb::TransferType;
using usb::UsageType;

namespace extensions {

namespace {

const char kDataKey[] = "data";
const char kResultCodeKey[] = "resultCode";

const char kErrorInitService[] = "Failed to initialize USB service.";

const char kErrorOpen[] = "Failed to open device.";
const char kErrorCancelled[] = "Transfer was cancelled.";
const char kErrorDisconnect[] = "Device disconnected.";
const char kErrorGeneric[] = "Transfer failed.";
const char kErrorNotSupported[] = "Not supported on this platform.";
const char kErrorNotConfigured[] = "The device is not in a configured state.";
const char kErrorOverflow[] = "Inbound transfer overflow.";
const char kErrorStalled[] = "Transfer stalled.";
const char kErrorTimeout[] = "Transfer timed out.";
const char kErrorTransferLength[] = "Transfer length is insufficient.";
const char kErrorCannotSetConfiguration[] =
    "Error setting device configuration.";
const char kErrorCannotClaimInterface[] = "Error claiming interface.";
const char kErrorCannotReleaseInterface[] = "Error releasing interface.";
const char kErrorCannotSetInterfaceAlternateSetting[] =
    "Error setting alternate interface setting.";
const char kErrorConvertDirection[] = "Invalid transfer direction.";
const char kErrorConvertRecipient[] = "Invalid transfer recipient.";
const char kErrorConvertRequestType[] = "Invalid request type.";
const char kErrorMalformedParameters[] = "Error parsing parameters.";
const char kErrorNoConnection[] = "No such connection.";
const char kErrorNoDevice[] = "No such device.";
const char kErrorPermissionDenied[] = "Permission to access device was denied";
const char kErrorInvalidTransferLength[] =
    "Transfer length must be a positive number less than 104,857,600.";
const char kErrorInvalidNumberOfPackets[] =
    "Number of packets must be a positive number less than 4,194,304.";
const char kErrorInvalidPacketLength[] =
    "Packet length must be a positive number less than 65,536.";
const char kErrorInvalidTimeout[] =
    "Transfer timeout must be greater than or equal to 0.";
const char kErrorResetDevice[] =
    "Error resetting the device. The device has been closed.";

const size_t kMaxTransferLength = 100 * 1024 * 1024;
const int kMaxPackets = 4 * 1024 * 1024;
const int kMaxPacketLength = 64 * 1024;

bool ConvertDirectionFromApi(const Direction& input,
                             UsbTransferDirection* output) {
  switch (input) {
    case usb::DIRECTION_IN:
      *output = UsbTransferDirection::INBOUND;
      return true;
    case usb::DIRECTION_OUT:
      *output = UsbTransferDirection::OUTBOUND;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertRequestTypeFromApi(const RequestType& input,
                               UsbControlTransferType* output) {
  switch (input) {
    case usb::REQUEST_TYPE_STANDARD:
      *output = UsbControlTransferType::STANDARD;
      return true;
    case usb::REQUEST_TYPE_CLASS:
      *output = UsbControlTransferType::CLASS;
      return true;
    case usb::REQUEST_TYPE_VENDOR:
      *output = UsbControlTransferType::VENDOR;
      return true;
    case usb::REQUEST_TYPE_RESERVED:
      *output = UsbControlTransferType::RESERVED;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool ConvertRecipientFromApi(const Recipient& input,
                             UsbControlTransferRecipient* output) {
  switch (input) {
    case usb::RECIPIENT_DEVICE:
      *output = UsbControlTransferRecipient::DEVICE;
      return true;
    case usb::RECIPIENT_INTERFACE:
      *output = UsbControlTransferRecipient::INTERFACE;
      return true;
    case usb::RECIPIENT_ENDPOINT:
      *output = UsbControlTransferRecipient::ENDPOINT;
      return true;
    case usb::RECIPIENT_OTHER:
      *output = UsbControlTransferRecipient::OTHER;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

template <class T>
bool GetTransferInSize(const T& input, uint32_t* output) {
  const int* length = input.length.get();
  if (length && *length >= 0 &&
      static_cast<uint32_t>(*length) < kMaxTransferLength) {
    *output = *length;
    return true;
  }
  return false;
}

const char* ConvertTransferStatusToApi(const UsbTransferStatus status) {
  switch (status) {
    case UsbTransferStatus::COMPLETED:
      return "";
    case UsbTransferStatus::TRANSFER_ERROR:
      return kErrorGeneric;
    case UsbTransferStatus::TIMEOUT:
      return kErrorTimeout;
    case UsbTransferStatus::CANCELLED:
      return kErrorCancelled;
    case UsbTransferStatus::STALLED:
      return kErrorStalled;
    case UsbTransferStatus::DISCONNECT:
      return kErrorDisconnect;
    case UsbTransferStatus::BABBLE:
      return kErrorOverflow;
    case UsbTransferStatus::SHORT_PACKET:
      return kErrorTransferLength;
    default:
      NOTREACHED();
      return "";
  }
}

std::unique_ptr<base::Value> PopulateConnectionHandle(int handle,
                                                      int vendor_id,
                                                      int product_id) {
  ConnectionHandle result;
  result.handle = handle;
  result.vendor_id = vendor_id;
  result.product_id = product_id;
  return result.ToValue();
}

TransferType ConvertTransferTypeToApi(const UsbTransferType& input) {
  switch (input) {
    case UsbTransferType::CONTROL:
      return usb::TRANSFER_TYPE_CONTROL;
    case UsbTransferType::INTERRUPT:
      return usb::TRANSFER_TYPE_INTERRUPT;
    case UsbTransferType::ISOCHRONOUS:
      return usb::TRANSFER_TYPE_ISOCHRONOUS;
    case UsbTransferType::BULK:
      return usb::TRANSFER_TYPE_BULK;
    default:
      NOTREACHED();
      return usb::TRANSFER_TYPE_NONE;
  }
}

Direction ConvertDirectionToApi(const UsbTransferDirection& input) {
  switch (input) {
    case UsbTransferDirection::INBOUND:
      return usb::DIRECTION_IN;
    case UsbTransferDirection::OUTBOUND:
      return usb::DIRECTION_OUT;
    default:
      NOTREACHED();
      return usb::DIRECTION_NONE;
  }
}

SynchronizationType ConvertSynchronizationTypeToApi(
    const UsbSynchronizationType& input) {
  switch (input) {
    case UsbSynchronizationType::NONE:
      return usb::SYNCHRONIZATION_TYPE_NONE;
    case UsbSynchronizationType::ASYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
    case UsbSynchronizationType::ADAPTIVE:
      return usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
    case UsbSynchronizationType::SYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
    default:
      NOTREACHED();
      return usb::SYNCHRONIZATION_TYPE_NONE;
  }
}

usb::UsageType ConvertUsageTypeToApi(const UsbUsageType& input) {
  switch (input) {
    case UsbUsageType::DATA:
      return usb::USAGE_TYPE_DATA;
    case UsbUsageType::FEEDBACK:
      return usb::USAGE_TYPE_FEEDBACK;
    case UsbUsageType::EXPLICIT_FEEDBACK:
      return usb::USAGE_TYPE_EXPLICITFEEDBACK;
    case UsbUsageType::PERIODIC:
      return usb::USAGE_TYPE_PERIODIC;
    case UsbUsageType::NOTIFICATION:
      return usb::USAGE_TYPE_NOTIFICATION;
    case UsbUsageType::RESERVED:
      return usb::USAGE_TYPE_NONE;
    default:
      NOTREACHED();
      return usb::USAGE_TYPE_NONE;
  }
}

EndpointDescriptor ConvertEndpointDescriptor(
    const device::mojom::UsbEndpointInfo& input) {
  EndpointDescriptor output;
  output.address = device::ConvertEndpointNumberToAddress(input);
  output.type = ConvertTransferTypeToApi(input.type);
  output.direction = ConvertDirectionToApi(input.direction);
  output.maximum_packet_size = input.packet_size;
  output.synchronization =
      ConvertSynchronizationTypeToApi(input.synchronization_type);
  output.usage = ConvertUsageTypeToApi(input.usage_type);
  output.polling_interval.reset(new int(input.polling_interval));
  output.extra_data.assign(input.extra_data.begin(), input.extra_data.end());
  return output;
}

InterfaceDescriptor ConvertInterfaceDescriptor(
    uint8_t interface_number,
    const device::mojom::UsbAlternateInterfaceInfo& input) {
  InterfaceDescriptor output;
  output.interface_number = interface_number;
  output.alternate_setting = input.alternate_setting;
  output.interface_class = input.class_code;
  output.interface_subclass = input.subclass_code;
  output.interface_protocol = input.protocol_code;
  for (const auto& input_endpoint : input.endpoints) {
    DCHECK(input_endpoint);
    output.endpoints.push_back(ConvertEndpointDescriptor(*input_endpoint));
  }
  output.extra_data.assign(input.extra_data.begin(), input.extra_data.end());
  return output;
}

ConfigDescriptor ConvertConfigDescriptor(
    const device::mojom::UsbConfigurationInfo& input) {
  ConfigDescriptor output;
  output.configuration_value = input.configuration_value;
  output.self_powered = input.self_powered;
  output.remote_wakeup = input.remote_wakeup;
  output.max_power = input.maximum_power;
  for (const auto& input_interface : input.interfaces) {
    DCHECK(input_interface);
    // device::mojom::UsbInterfaceInfo aggregated all alternate settings
    // with the same interface number.
    for (const auto& alternate : input_interface->alternates) {
      DCHECK(alternate);
      output.interfaces.push_back(ConvertInterfaceDescriptor(
          input_interface->interface_number, *alternate));
    }
  }
  output.extra_data.assign(input.extra_data.begin(), input.extra_data.end());
  return output;
}

device::mojom::UsbDeviceFilterPtr ConvertDeviceFilter(
    const usb::DeviceFilter& input) {
  auto output = device::mojom::UsbDeviceFilter::New();
  if (input.vendor_id) {
    output->has_vendor_id = true;
    output->vendor_id = *input.vendor_id;
  }
  if (input.product_id) {
    output->has_product_id = true;
    output->product_id = *input.product_id;
  }
  if (input.interface_class) {
    output->has_class_code = true;
    output->class_code = *input.interface_class;
  }
  if (input.interface_subclass) {
    output->has_subclass_code = true;
    output->subclass_code = *input.interface_subclass;
  }
  if (input.interface_protocol) {
    output->has_protocol_code = true;
    output->protocol_code = *input.interface_protocol;
  }
  return output;
}

}  // namespace

UsbExtensionFunction::UsbExtensionFunction() = default;
UsbExtensionFunction::~UsbExtensionFunction() = default;

UsbDeviceManager* UsbExtensionFunction::usb_device_manager() {
  if (!usb_device_manager_) {
    usb_device_manager_ = UsbDeviceManager::Get(browser_context());
  }

  return usb_device_manager_;
}

UsbPermissionCheckingFunction::UsbPermissionCheckingFunction()
    : device_permissions_manager_(nullptr) {}

UsbPermissionCheckingFunction::~UsbPermissionCheckingFunction() = default;

bool UsbPermissionCheckingFunction::HasDevicePermission(
    const device::mojom::UsbDeviceInfo& device) {
  if (!device_permissions_manager_) {
    device_permissions_manager_ =
        DevicePermissionsManager::Get(browser_context());
  }

  DevicePermissions* device_permissions =
      device_permissions_manager_->GetForExtension(extension_id());
  DCHECK(device_permissions);

  permission_entry_ = device_permissions->FindUsbDeviceEntry(device);
  if (permission_entry_.get()) {
    return true;
  }

  std::unique_ptr<UsbDevicePermission::CheckParam> param =
      UsbDevicePermission::CheckParam::ForUsbDevice(extension(), device);
  if (extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, param.get())) {
    return true;
  }

  return false;
}

void UsbPermissionCheckingFunction::RecordDeviceLastUsed() {
  if (permission_entry_.get()) {
    device_permissions_manager_->UpdateLastUsed(extension_id(),
                                                permission_entry_);
  }
}

UsbConnectionFunction::UsbConnectionFunction() = default;
UsbConnectionFunction::~UsbConnectionFunction() = default;

UsbDeviceResource* UsbConnectionFunction::GetResourceFromHandle(
    const ConnectionHandle& handle) {
  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  if (!manager) {
    return nullptr;
  }
  return manager->Get(extension_id(), handle.handle);
}

device::mojom::UsbDevice* UsbConnectionFunction::GetDeviceFromHandle(
    const ConnectionHandle& handle) {
  UsbDeviceResource* resource = GetResourceFromHandle(handle);
  if (!resource) {
    return nullptr;
  }

  return resource->device();
}

const device::mojom::UsbDeviceInfo*
UsbConnectionFunction::GetDeviceInfoFromHandle(const ConnectionHandle& handle) {
  UsbDeviceResource* resource = GetResourceFromHandle(handle);
  if (!resource || !resource->device()) {
    return nullptr;
  }

  auto* device_manager = usb_device_manager();
  if (!device_manager) {
    return nullptr;
  }

  return device_manager->GetDeviceInfo(resource->guid());
}

void UsbConnectionFunction::ReleaseDeviceResource(
    const ConnectionHandle& handle) {
  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  manager->Remove(extension_id(), handle.handle);
}

UsbTransferFunction::UsbTransferFunction() = default;
UsbTransferFunction::~UsbTransferFunction() = default;

void UsbTransferFunction::OnCompleted(
    UsbTransferStatus status,
    std::unique_ptr<base::DictionaryValue> transfer_info) {
  if (status == UsbTransferStatus::COMPLETED) {
    Respond(OneArgument(std::move(transfer_info)));
  } else {
    auto error_args = std::make_unique<base::ListValue>();
    error_args->Append(std::move(transfer_info));
    // Using ErrorWithArguments is discouraged but required to provide the
    // detailed transfer info as the transfer may have partially succeeded.
    Respond(ErrorWithArguments(std::move(error_args),
                               ConvertTransferStatusToApi(status)));
  }
}

void UsbTransferFunction::OnTransferInCompleted(
    UsbTransferStatus status,
    const std::vector<uint8_t>& data) {
  auto transfer_info = std::make_unique<base::DictionaryValue>();
  transfer_info->SetInteger(kResultCodeKey, static_cast<int>(status));
  transfer_info->Set(
      kDataKey, base::Value::CreateWithCopiedBuffer(
                    reinterpret_cast<const char*>(data.data()), data.size()));

  OnCompleted(status, std::move(transfer_info));
}

void UsbTransferFunction::OnTransferOutCompleted(UsbTransferStatus status) {
  auto transfer_info = std::make_unique<base::DictionaryValue>();
  transfer_info->SetInteger(kResultCodeKey, static_cast<int>(status));
  transfer_info->Set(kDataKey,
                     std::make_unique<base::Value>(base::Value::Type::BINARY));

  OnCompleted(status, std::move(transfer_info));
}

void UsbTransferFunction::OnDisconnect() {
  const auto status = UsbTransferStatus::DISCONNECT;
  auto transfer_info = std::make_unique<base::DictionaryValue>();
  transfer_info->SetInteger(kResultCodeKey, static_cast<int>(status));
  OnCompleted(status, std::move(transfer_info));
}

UsbGenericTransferFunction::UsbGenericTransferFunction() = default;
UsbGenericTransferFunction::~UsbGenericTransferFunction() = default;

// const usb::InterruptTransfer::Params* or
// const usb::BulkTransfer::Params*
template <typename T>
ExtensionFunction::ResponseAction UsbGenericTransferFunction::DoTransfer(
    T params) {
  device::mojom::UsbDevice* device = GetDeviceFromHandle(params->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const GenericTransferInfo& transfer = params->transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;

  if (!ConvertDirectionFromApi(transfer.direction, &direction)) {
    return RespondNow(Error(kErrorConvertDirection));
  }

  int timeout = transfer.timeout ? *transfer.timeout : 0;
  if (timeout < 0) {
    return RespondNow(Error(kErrorInvalidTimeout));
  }

  if (direction == UsbTransferDirection::INBOUND) {
    uint32_t size = 0;
    if (!GetTransferInSize(transfer, &size)) {
      return RespondNow(Error(kErrorInvalidTransferLength));
    }

    device->GenericTransferIn(
        transfer.endpoint, size, timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(&UsbGenericTransferFunction::OnTransferInCompleted,
                           this),
            base::BindOnce(&UsbGenericTransferFunction::OnDisconnect, this)));
  } else {
    // For case direction == UsbTransferDirection::OUTBOUND.
    if (!transfer.data) {
      return RespondNow(Error(kErrorMalformedParameters));
    }

    device->GenericTransferOut(
        transfer.endpoint, *transfer.data, timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(&UsbGenericTransferFunction::OnTransferOutCompleted,
                           this),
            base::BindOnce(&UsbGenericTransferFunction::OnDisconnect, this)));
  }
  return RespondLater();
}

UsbFindDevicesFunction::UsbFindDevicesFunction() = default;
UsbFindDevicesFunction::~UsbFindDevicesFunction() = default;

ExtensionFunction::ResponseAction UsbFindDevicesFunction::Run() {
  std::unique_ptr<usb::FindDevices::Params> parameters =
      FindDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  vendor_id_ = parameters->options.vendor_id;
  product_id_ = parameters->options.product_id;
  int interface_id = parameters->options.interface_id.get()
                         ? *parameters->options.interface_id
                         : UsbDevicePermissionData::SPECIAL_VALUE_ANY;
  // Bail out early if there is no chance that the app has manifest permission
  // for the USB device described by vendor ID, product ID, and interface ID.
  // Note that this will match any permission filter that has only interface
  // class specified - in order to match interface class information about
  // device interfaces is needed, which is not known at this point; the
  // permission will have to be checked again when the USB device info is
  // fetched.
  std::unique_ptr<UsbDevicePermission::CheckParam> param =
      UsbDevicePermission::CheckParam::ForDeviceWithAnyInterfaceClass(
          extension(), vendor_id_, product_id_, interface_id);
  if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
          APIPermission::kUsbDevice, param.get())) {
    return RespondNow(Error(kErrorPermissionDenied));
  }

  auto* device_manager = usb_device_manager();
  if (!device_manager) {
    return RespondNow(Error(kErrorInitService));
  }

  device_manager->GetDevices(
      base::BindOnce(&UsbFindDevicesFunction::OnGetDevicesComplete, this));
  return RespondLater();
}

void UsbFindDevicesFunction::OnGetDevicesComplete(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  result_.reset(new base::ListValue());
  barrier_ = base::BarrierClosure(
      devices.size(),
      base::BindOnce(&UsbFindDevicesFunction::OpenComplete, this));

  for (const auto& device_info : devices) {
    // Skip the device whose vendor and product ID do not match the target one.
    if (device_info->vendor_id != vendor_id_ ||
        device_info->product_id != product_id_) {
      barrier_.Run();
      continue;
    }

    // Verify that the app has permission for the device again, this time taking
    // device's interface classes into account - in case there is a USB device
    // permission specifying only interfaceClass, permissions check in |Run|
    // might have passed even though the app did not have permission for
    // specified vendor and product ID (as actual permissions check had to be
    // deferred until device's interface classes are known).
    std::unique_ptr<UsbDevicePermission::CheckParam> param =
        UsbDevicePermission::CheckParam::ForUsbDevice(extension(),
                                                      *device_info);
    if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
            APIPermission::kUsbDevice, param.get())) {
      barrier_.Run();
    } else {
      mojo::Remote<device::mojom::UsbDevice> device;
      usb_device_manager()->GetDevice(device_info->guid,
                                      device.BindNewPipeAndPassReceiver());
      auto* device_raw = device.get();
      device_raw->Open(mojo::WrapCallbackWithDropHandler(
          base::BindOnce(&UsbFindDevicesFunction::OnDeviceOpened, this,
                         device_info->guid, std::move(device)),
          base::BindOnce(&UsbFindDevicesFunction::OnDisconnect, this)));
    }
  }
}

void UsbFindDevicesFunction::OnDeviceOpened(
    const std::string& guid,
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbOpenDeviceError error) {
  if (error == device::mojom::UsbOpenDeviceError::OK && device) {
    ApiResourceManager<UsbDeviceResource>* manager =
        ApiResourceManager<UsbDeviceResource>::Get(browser_context());
    UsbDeviceResource* resource =
        new UsbDeviceResource(extension_id(), guid, std::move(device));
    result_->Append(PopulateConnectionHandle(manager->Add(resource), vendor_id_,
                                             product_id_));
  }
  barrier_.Run();
}

void UsbFindDevicesFunction::OnDisconnect() {
  barrier_.Run();
}

void UsbFindDevicesFunction::OpenComplete() {
  Respond(OneArgument(std::move(result_)));
}

UsbGetDevicesFunction::UsbGetDevicesFunction() = default;
UsbGetDevicesFunction::~UsbGetDevicesFunction() = default;

ExtensionFunction::ResponseAction UsbGetDevicesFunction::Run() {
  std::unique_ptr<usb::GetDevices::Params> parameters =
      GetDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  if (parameters->options.filters) {
    filters_.reserve(parameters->options.filters->size());
    for (const auto& filter : *parameters->options.filters)
      filters_.push_back(ConvertDeviceFilter(filter));
  }
  if (parameters->options.vendor_id) {
    auto filter = device::mojom::UsbDeviceFilter::New();
    filter->has_vendor_id = true;
    filter->vendor_id = *parameters->options.vendor_id;
    if (parameters->options.product_id) {
      filter->has_product_id = true;
      filter->product_id = *parameters->options.product_id;
    }
    filters_.push_back(std::move(filter));
  }

  auto* device_manager = usb_device_manager();
  if (!device_manager) {
    return RespondNow(Error(kErrorInitService));
  }

  device_manager->GetDevices(
      base::BindOnce(&UsbGetDevicesFunction::OnGetDevicesComplete, this));
  return RespondLater();
}

void UsbGetDevicesFunction::OnGetDevicesComplete(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (const auto& device : devices) {
    if (device::UsbDeviceFilterMatchesAny(filters_, *device) &&
        HasDevicePermission(*device)) {
      Device api_device;
      usb_device_manager()->GetApiDevice(*device, &api_device);
      result->Append(api_device.ToValue());
    }
  }

  Respond(OneArgument(std::move(result)));
}

UsbGetUserSelectedDevicesFunction::UsbGetUserSelectedDevicesFunction() =
    default;
UsbGetUserSelectedDevicesFunction::~UsbGetUserSelectedDevicesFunction() =
    default;

ExtensionFunction::ResponseAction UsbGetUserSelectedDevicesFunction::Run() {
  std::unique_ptr<usb::GetUserSelectedDevices::Params> parameters =
      GetUserSelectedDevices::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  if (!user_gesture()) {
    return RespondNow(OneArgument(std::make_unique<base::ListValue>()));
  }

  bool multiple = false;
  if (parameters->options.multiple) {
    multiple = *parameters->options.multiple;
  }

  std::vector<UsbDeviceFilterPtr> filters;
  if (parameters->options.filters) {
    filters.reserve(parameters->options.filters->size());
    for (const auto& filter : *parameters->options.filters)
      filters.push_back(ConvertDeviceFilter(filter));
  }

  content::WebContents* web_contents = GetSenderWebContents();
  if (!web_contents) {
    return RespondNow(
        Error(function_constants::kCouldNotFindSenderWebContents));
  }

  prompt_ =
      ExtensionsAPIClient::Get()->CreateDevicePermissionsPrompt(web_contents);
  if (!prompt_) {
    return RespondNow(Error(kErrorNotSupported));
  }

  prompt_->AskForUsbDevices(
      extension(), browser_context(), multiple, std::move(filters),
      base::Bind(&UsbGetUserSelectedDevicesFunction::OnDevicesChosen, this));
  return RespondLater();
}

void UsbGetUserSelectedDevicesFunction::OnDevicesChosen(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  auto* device_manager = usb_device_manager();
  DCHECK(device_manager);

  for (const auto& device : devices) {
    Device api_device;
    device_manager->GetApiDevice(*device, &api_device);
    result->Append(api_device.ToValue());
  }

  Respond(OneArgument(std::move(result)));
}

UsbGetConfigurationsFunction::UsbGetConfigurationsFunction() = default;
UsbGetConfigurationsFunction::~UsbGetConfigurationsFunction() = default;

ExtensionFunction::ResponseAction UsbGetConfigurationsFunction::Run() {
  std::unique_ptr<usb::GetConfigurations::Params> parameters =
      GetConfigurations::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  auto* device_manager = usb_device_manager();
  if (!device_manager) {
    return RespondNow(Error(kErrorInitService));
  }

  std::string guid;
  if (!device_manager->GetGuidFromId(parameters->device.device, &guid)) {
    return RespondNow(Error(kErrorNoDevice));
  }

  const auto* device_info = device_manager->GetDeviceInfo(guid);
  if (!device_info) {
    return RespondNow(Error(kErrorNoDevice));
  }

  if (!HasDevicePermission(*device_info)) {
    // This function must act as if there is no such device. Otherwise it can be
    // used to fingerprint unauthorized devices.
    return RespondNow(Error(kErrorNoDevice));
  }

  std::unique_ptr<base::ListValue> configs(new base::ListValue());
  uint8_t active_config_value = device_info->active_configuration;
  for (const auto& config : device_info->configurations) {
    DCHECK(config);
    ConfigDescriptor api_config = ConvertConfigDescriptor(*config);
    if (active_config_value &&
        config->configuration_value == active_config_value) {
      api_config.active = true;
    }
    configs->Append(api_config.ToValue());
  }
  return RespondNow(OneArgument(std::move(configs)));
}

UsbRequestAccessFunction::UsbRequestAccessFunction() = default;
UsbRequestAccessFunction::~UsbRequestAccessFunction() = default;

ExtensionFunction::ResponseAction UsbRequestAccessFunction::Run() {
  std::unique_ptr<usb::RequestAccess::Params> parameters =
      RequestAccess::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

UsbOpenDeviceFunction::UsbOpenDeviceFunction() = default;
UsbOpenDeviceFunction::~UsbOpenDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbOpenDeviceFunction::Run() {
  std::unique_ptr<usb::OpenDevice::Params> parameters =
      OpenDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  auto* device_manager = usb_device_manager();
  if (!device_manager) {
    return RespondNow(Error(kErrorInitService));
  }

  std::string guid;
  if (!device_manager->GetGuidFromId(parameters->device.device, &guid)) {
    return RespondNow(Error(kErrorNoDevice));
  }

  const device::mojom::UsbDeviceInfo* device_info =
      device_manager->GetDeviceInfo(guid);
  if (!device_info) {
    return RespondNow(Error(kErrorNoDevice));
  }

  if (!HasDevicePermission(*device_info)) {
    // This function must act as if there is no such device. Otherwise it can be
    // used to fingerprint unauthorized devices.
    return RespondNow(Error(kErrorNoDevice));
  }

  mojo::Remote<device::mojom::UsbDevice> device;
  device_manager->GetDevice(device_info->guid,
                            device.BindNewPipeAndPassReceiver());
  auto* device_raw = device.get();
  device_raw->Open(mojo::WrapCallbackWithDropHandler(
      base::BindOnce(&UsbOpenDeviceFunction::OnDeviceOpened, this,
                     device_info->guid, std::move(device)),
      base::BindOnce(&UsbOpenDeviceFunction::OnDisconnect, this)));
  return RespondLater();
}

void UsbOpenDeviceFunction::OnDeviceOpened(
    std::string guid,
    mojo::Remote<device::mojom::UsbDevice> device,
    device::mojom::UsbOpenDeviceError error) {
  if (error != device::mojom::UsbOpenDeviceError::OK || !device) {
    Respond(Error(kErrorOpen));
    return;
  }

  RecordDeviceLastUsed();

  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  const device::mojom::UsbDeviceInfo* device_info =
      usb_device_manager()->GetDeviceInfo(guid);
  DCHECK(device_info);
  UsbDeviceResource* resource = new UsbDeviceResource(
      extension_id(), device_info->guid, std::move(device));
  Respond(OneArgument(PopulateConnectionHandle(manager->Add(resource),
                                               device_info->vendor_id,
                                               device_info->product_id)));
}

void UsbOpenDeviceFunction::OnDisconnect() {
  Respond(Error(kErrorDisconnect));
}

UsbSetConfigurationFunction::UsbSetConfigurationFunction() = default;
UsbSetConfigurationFunction::~UsbSetConfigurationFunction() = default;

ExtensionFunction::ResponseAction UsbSetConfigurationFunction::Run() {
  std::unique_ptr<usb::SetConfiguration::Params> parameters =
      SetConfiguration::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  UsbDeviceResource* resource = GetResourceFromHandle(parameters->handle);
  if (!resource || !resource->device()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  if (parameters->configuration_value < 0) {
    return RespondNow(Error(kErrorMalformedParameters));
  }

  uint8_t config_value = parameters->configuration_value;
  resource->device()->SetConfiguration(
      config_value, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                        base::BindOnce(&UsbSetConfigurationFunction::OnComplete,
                                       this, resource->guid(), config_value),
                        false));
  return RespondLater();
}

void UsbSetConfigurationFunction::OnComplete(const std::string& guid,
                                             uint8_t config_value,
                                             bool success) {
  if (success) {
    bool updated_config =
        usb_device_manager()->UpdateActiveConfig(guid, config_value);
    DCHECK(updated_config);
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotSetConfiguration));
  }
}

UsbGetConfigurationFunction::UsbGetConfigurationFunction() = default;
UsbGetConfigurationFunction::~UsbGetConfigurationFunction() = default;

ExtensionFunction::ResponseAction UsbGetConfigurationFunction::Run() {
  std::unique_ptr<usb::GetConfiguration::Params> parameters =
      GetConfiguration::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  const device::mojom::UsbDeviceInfo* device_info =
      GetDeviceInfoFromHandle(parameters->handle);
  if (!device_info) {
    return RespondNow(Error(kErrorNoConnection));
  }

  uint8_t active_config_value = device_info->active_configuration;
  if (active_config_value) {
    for (const auto& config : device_info->configurations) {
      DCHECK(config);
      if (config->configuration_value == active_config_value) {
        ConfigDescriptor api_config = ConvertConfigDescriptor(*config);
        return RespondNow(OneArgument(api_config.ToValue()));
      }
    }
  }
  // Respond with an error if there is no active config or the config object
  // can't be found according to |active_config_value|.
  return RespondNow(Error(kErrorNotConfigured));
}

UsbListInterfacesFunction::UsbListInterfacesFunction() = default;
UsbListInterfacesFunction::~UsbListInterfacesFunction() = default;

ExtensionFunction::ResponseAction UsbListInterfacesFunction::Run() {
  std::unique_ptr<usb::ListInterfaces::Params> parameters =
      ListInterfaces::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  const device::mojom::UsbDeviceInfo* device_info =
      GetDeviceInfoFromHandle(parameters->handle);
  if (!device_info) {
    return RespondNow(Error(kErrorNoConnection));
  }

  uint8_t active_config_value = device_info->active_configuration;
  if (!active_config_value) {
    return RespondNow(Error(kErrorNotConfigured));
  }

  for (const auto& config : device_info->configurations) {
    DCHECK(config);
    if (config->configuration_value == active_config_value) {
      ConfigDescriptor api_config = ConvertConfigDescriptor(*config);
      std::unique_ptr<base::ListValue> result(new base::ListValue);
      for (size_t i = 0; i < api_config.interfaces.size(); ++i) {
        result->Append(api_config.interfaces[i].ToValue());
      }
      return RespondNow(OneArgument(std::move(result)));
    }
  }
  // Respond with an error if the config object can't be found according to
  // |active_config_value|.
  return RespondNow(Error(kErrorNotConfigured));
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() = default;
UsbCloseDeviceFunction::~UsbCloseDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbCloseDeviceFunction::Run() {
  std::unique_ptr<usb::CloseDevice::Params> parameters =
      CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  // The device handle is closed when the resource is destroyed.
  ReleaseDeviceResource(parameters->handle);
  return RespondNow(NoArguments());
}

UsbClaimInterfaceFunction::UsbClaimInterfaceFunction() = default;
UsbClaimInterfaceFunction::~UsbClaimInterfaceFunction() = default;

ExtensionFunction::ResponseAction UsbClaimInterfaceFunction::Run() {
  std::unique_ptr<usb::ClaimInterface::Params> parameters =
      ClaimInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device->ClaimInterface(
      parameters->interface_number,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UsbClaimInterfaceFunction::OnComplete, this), false));
  return RespondLater();
}

void UsbClaimInterfaceFunction::OnComplete(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotClaimInterface));
  }
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() = default;
UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() = default;

ExtensionFunction::ResponseAction UsbReleaseInterfaceFunction::Run() {
  std::unique_ptr<usb::ReleaseInterface::Params> parameters =
      ReleaseInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device->ReleaseInterface(
      parameters->interface_number,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UsbReleaseInterfaceFunction::OnComplete, this),
          false));
  return RespondLater();
}

void UsbReleaseInterfaceFunction::OnComplete(bool success) {
  if (success)
    Respond(NoArguments());
  else
    Respond(Error(kErrorCannotReleaseInterface));
}

UsbSetInterfaceAlternateSettingFunction::
    UsbSetInterfaceAlternateSettingFunction() = default;

UsbSetInterfaceAlternateSettingFunction::
    ~UsbSetInterfaceAlternateSettingFunction() = default;

ExtensionFunction::ResponseAction
UsbSetInterfaceAlternateSettingFunction::Run() {
  std::unique_ptr<usb::SetInterfaceAlternateSetting::Params> parameters =
      SetInterfaceAlternateSetting::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device->SetInterfaceAlternateSetting(
      parameters->interface_number, parameters->alternate_setting,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UsbSetInterfaceAlternateSettingFunction::OnComplete,
                         this),
          false));
  return RespondLater();
}

void UsbSetInterfaceAlternateSettingFunction::OnComplete(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotSetInterfaceAlternateSetting));
  }
}

UsbControlTransferFunction::UsbControlTransferFunction() = default;
UsbControlTransferFunction::~UsbControlTransferFunction() = default;

ExtensionFunction::ResponseAction UsbControlTransferFunction::Run() {
  std::unique_ptr<usb::ControlTransfer::Params> parameters =
      ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const ControlTransferInfo& transfer = parameters->transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;
  UsbControlTransferType request_type;
  UsbControlTransferRecipient recipient;

  if (!ConvertDirectionFromApi(transfer.direction, &direction)) {
    return RespondNow(Error(kErrorConvertDirection));
  }

  if (!ConvertRequestTypeFromApi(transfer.request_type, &request_type)) {
    return RespondNow(Error(kErrorConvertRequestType));
  }

  if (!ConvertRecipientFromApi(transfer.recipient, &recipient)) {
    return RespondNow(Error(kErrorConvertRecipient));
  }

  int timeout = transfer.timeout ? *transfer.timeout : 0;
  if (timeout < 0) {
    return RespondNow(Error(kErrorInvalidTimeout));
  }

  auto mojo_parameters =
      UsbControlTransferParams::New(request_type, recipient, transfer.request,
                                    transfer.value, transfer.index);

  if (direction == UsbTransferDirection::INBOUND) {
    uint32_t size = 0;
    if (!GetTransferInSize(transfer, &size)) {
      return RespondNow(Error(kErrorInvalidTransferLength));
    }

    device->ControlTransferIn(
        std::move(mojo_parameters), size, timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(&UsbControlTransferFunction::OnTransferInCompleted,
                           this),
            base::BindOnce(&UsbControlTransferFunction::OnDisconnect, this)));
  } else {
    // For case direction == UsbTransferDirection::OUTBOUND.
    if (!transfer.data) {
      return RespondNow(Error(kErrorMalformedParameters));
    }

    device->ControlTransferOut(
        std::move(mojo_parameters), *transfer.data, timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(&UsbControlTransferFunction::OnTransferOutCompleted,
                           this),
            base::BindOnce(&UsbControlTransferFunction::OnDisconnect, this)));
  }
  return RespondLater();
}

UsbBulkTransferFunction::UsbBulkTransferFunction() = default;
UsbBulkTransferFunction::~UsbBulkTransferFunction() = default;

ExtensionFunction::ResponseAction UsbBulkTransferFunction::Run() {
  std::unique_ptr<usb::BulkTransfer::Params> parameters =
      BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  return DoTransfer<const usb::BulkTransfer::Params*>(parameters.get());
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() = default;
UsbInterruptTransferFunction::~UsbInterruptTransferFunction() = default;

ExtensionFunction::ResponseAction UsbInterruptTransferFunction::Run() {
  std::unique_ptr<usb::InterruptTransfer::Params> parameters =
      InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  return DoTransfer<const usb::InterruptTransfer::Params*>(parameters.get());
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() = default;
UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() = default;

ExtensionFunction::ResponseAction UsbIsochronousTransferFunction::Run() {
  std::unique_ptr<usb::IsochronousTransfer::Params> parameters =
      IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const IsochronousTransferInfo& transfer = parameters->transfer_info;
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;

  if (!ConvertDirectionFromApi(generic_transfer.direction, &direction))
    return RespondNow(Error(kErrorConvertDirection));

  uint32_t size = 0;
  if (direction == UsbTransferDirection::INBOUND) {
    if (!GetTransferInSize(generic_transfer, &size))
      return RespondNow(Error(kErrorInvalidTransferLength));
  } else {
    if (!generic_transfer.data)
      return RespondNow(Error(kErrorMalformedParameters));

    size = generic_transfer.data->size();
  }

  if (transfer.packets < 0 || transfer.packets >= kMaxPackets)
    return RespondNow(Error(kErrorInvalidNumberOfPackets));
  size_t packets = transfer.packets;

  if (transfer.packet_length < 0 ||
      transfer.packet_length >= kMaxPacketLength) {
    return RespondNow(Error(kErrorInvalidPacketLength));
  }

  size_t total_length = packets * transfer.packet_length;
  if (packets > size || total_length > size)
    return RespondNow(Error(kErrorTransferLength));

  std::vector<uint32_t> packet_lengths(packets, transfer.packet_length);

  int timeout = generic_transfer.timeout ? *generic_transfer.timeout : 0;
  if (timeout < 0)
    return RespondNow(Error(kErrorInvalidTimeout));

  if (direction == UsbTransferDirection::INBOUND) {
    device->IsochronousTransferIn(
        generic_transfer.endpoint, packet_lengths, timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(
                &UsbIsochronousTransferFunction::OnTransferInCompleted, this),
            base::BindOnce(&UsbIsochronousTransferFunction::OnDisconnect,
                           this)));
  } else {
    device->IsochronousTransferOut(
        generic_transfer.endpoint, *generic_transfer.data, packet_lengths,
        timeout,
        mojo::WrapCallbackWithDropHandler(
            base::BindOnce(
                &UsbIsochronousTransferFunction::OnTransferOutCompleted, this),
            base::BindOnce(&UsbIsochronousTransferFunction::OnDisconnect,
                           this)));
  }
  return RespondLater();
}

void UsbIsochronousTransferFunction::OnTransferInCompleted(
    const std::vector<uint8_t>& data,
    std::vector<UsbIsochronousPacketPtr> packets) {
  size_t length = std::accumulate(packets.begin(), packets.end(), 0,
                                  [](const size_t& a, const auto& packet) {
                                    return a + packet->transferred_length;
                                  });
  std::vector<char> buffer;
  buffer.reserve(length);

  UsbTransferStatus status = UsbTransferStatus::COMPLETED;
  const char* data_ptr = reinterpret_cast<const char*>(data.data());
  for (const auto& packet : packets) {
    // Capture the error status of the first unsuccessful packet.
    if (status == UsbTransferStatus::COMPLETED &&
        packet->status != UsbTransferStatus::COMPLETED) {
      status = packet->status;
    }

    buffer.insert(buffer.end(), data_ptr,
                  data_ptr + packet->transferred_length);
    data_ptr += packet->length;
  }

  auto transfer_info = std::make_unique<base::DictionaryValue>();
  transfer_info->SetKey(kResultCodeKey, base::Value(static_cast<int>(status)));
  transfer_info->SetKey(kDataKey, base::Value(std::move(buffer)));
  OnCompleted(status, std::move(transfer_info));
}

void UsbIsochronousTransferFunction::OnTransferOutCompleted(
    std::vector<UsbIsochronousPacketPtr> packets) {
  UsbTransferStatus status = UsbTransferStatus::COMPLETED;
  for (const auto& packet : packets) {
    // Capture the error status of the first unsuccessful packet.
    if (status == UsbTransferStatus::COMPLETED &&
        packet->status != UsbTransferStatus::COMPLETED) {
      status = packet->status;
    }
  }
  auto transfer_info = std::make_unique<base::DictionaryValue>();
  transfer_info->SetKey(kResultCodeKey, base::Value(static_cast<int>(status)));
  OnCompleted(status, std::move(transfer_info));
}

UsbResetDeviceFunction::UsbResetDeviceFunction() = default;
UsbResetDeviceFunction::~UsbResetDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbResetDeviceFunction::Run() {
  parameters_ = ResetDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters_->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device->Reset(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&UsbResetDeviceFunction::OnComplete, this), false));
  return RespondLater();
}

void UsbResetDeviceFunction::OnComplete(bool success) {
  if (success) {
    Respond(OneArgument(std::make_unique<base::Value>(true)));
  } else {
    ReleaseDeviceResource(parameters_->handle);

    std::unique_ptr<base::ListValue> error_args(new base::ListValue());
    error_args->AppendBoolean(false);
    // Using ErrorWithArguments is discouraged but required to maintain
    // compatibility with existing applications.
    Respond(ErrorWithArguments(std::move(error_args), kErrorResetDevice));
  }
}

}  // namespace extensions
