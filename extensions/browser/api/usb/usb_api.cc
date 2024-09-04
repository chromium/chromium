// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/usb/usb_api.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/usb/usb_device_resource.h"
#include "extensions/browser/extension_function_constants.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/device/public/cpp/usb/usb_utils.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_enumeration_options.mojom.h"

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
using device::mojom::UsbClaimInterfaceResult;
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
    case usb::Direction::kIn:
      *output = UsbTransferDirection::INBOUND;
      return true;
    case usb::Direction::kOut:
      *output = UsbTransferDirection::OUTBOUND;
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool ConvertRequestTypeFromApi(const RequestType& input,
                               UsbControlTransferType* output) {
  switch (input) {
    case usb::RequestType::kStandard:
      *output = UsbControlTransferType::STANDARD;
      return true;
    case usb::RequestType::kClass:
      *output = UsbControlTransferType::CLASS;
      return true;
    case usb::RequestType::kVendor:
      *output = UsbControlTransferType::VENDOR;
      return true;
    case usb::RequestType::kReserved:
      *output = UsbControlTransferType::RESERVED;
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

bool ConvertRecipientFromApi(const Recipient& input,
                             UsbControlTransferRecipient* output) {
  switch (input) {
    case usb::Recipient::kDevice:
      *output = UsbControlTransferRecipient::DEVICE;
      return true;
    case usb::Recipient::kInterface:
      *output = UsbControlTransferRecipient::INTERFACE;
      return true;
    case usb::Recipient::kEndpoint:
      *output = UsbControlTransferRecipient::ENDPOINT;
      return true;
    case usb::Recipient::kOther:
      *output = UsbControlTransferRecipient::OTHER;
      return true;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

template <class T>
bool GetTransferInSize(const T& input, uint32_t* output) {
  const auto& length = input.length;
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
      DUMP_WILL_BE_NOTREACHED();
      return "";
  }
}

base::Value::Dict PopulateConnectionHandle(int handle,
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
      return usb::TransferType::kControl;
    case UsbTransferType::INTERRUPT:
      return usb::TransferType::kInterrupt;
    case UsbTransferType::ISOCHRONOUS:
      return usb::TransferType::kIsochronous;
    case UsbTransferType::BULK:
      return usb::TransferType::kBulk;
    default:
      NOTREACHED_IN_MIGRATION();
      return usb::TransferType::kNone;
  }
}

Direction ConvertDirectionToApi(const UsbTransferDirection& input) {
  switch (input) {
    case UsbTransferDirection::INBOUND:
      return usb::Direction::kIn;
    case UsbTransferDirection::OUTBOUND:
      return usb::Direction::kOut;
    default:
      NOTREACHED_IN_MIGRATION();
      return usb::Direction::kNone;
  }
}

SynchronizationType ConvertSynchronizationTypeToApi(
    const UsbSynchronizationType& input) {
  switch (input) {
    case UsbSynchronizationType::NONE:
      return usb::SynchronizationType::kNone;
    case UsbSynchronizationType::ASYNCHRONOUS:
      return usb::SynchronizationType::kAsynchronous;
    case UsbSynchronizationType::ADAPTIVE:
      return usb::SynchronizationType::kAdaptive;
    case UsbSynchronizationType::SYNCHRONOUS:
      return usb::SynchronizationType::kSynchronous;
    default:
      NOTREACHED_IN_MIGRATION();
      return usb::SynchronizationType::kNone;
  }
}

usb::UsageType ConvertUsageTypeToApi(const UsbUsageType& input) {
  switch (input) {
    case UsbUsageType::DATA:
      return usb::UsageType::kData;
    case UsbUsageType::FEEDBACK:
      return usb::UsageType::kFeedback;
    case UsbUsageType::EXPLICIT_FEEDBACK:
      return usb::UsageType::kExplicitFeedback;
    case UsbUsageType::PERIODIC:
      return usb::UsageType::kPeriodic;
    case UsbUsageType::NOTIFICATION:
      return usb::UsageType::kNotification;
    case UsbUsageType::RESERVED:
      return usb::UsageType::kNone;
    default:
      NOTREACHED_IN_MIGRATION();
      return usb::UsageType::kNone;
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
  output.polling_interval = input.polling_interval;
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

bool UsbExtensionFunction::IsUsbDeviceAllowedByPolicy(int vendor_id,
                                                      int product_id) {
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  DCHECK(client);
  return client->IsUsbDeviceAllowedByPolicy(browser_context(), extension_id(),
                                            vendor_id, product_id);
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
          mojom::APIPermissionID::kUsbDevice, param.get())) {
    return true;
  }

  if (IsUsbDeviceAllowedByPolicy(device.vendor_id, device.product_id)) {
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

void UsbTransferFunction::OnCompleted(UsbTransferStatus status,
                                      base::Value::Dict transfer_info) {
  if (status == UsbTransferStatus::COMPLETED) {
    Respond(WithArguments(std::move(transfer_info)));
  } else {
    base::Value::List error_args;
    error_args.Append(std::move(transfer_info));
    // Using ErrorWithArguments is discouraged but required to provide the
    // detailed transfer info as the transfer may have partially succeeded.
    Respond(ErrorWithArguments(std::move(error_args),
                               ConvertTransferStatusToApi(status)));
  }
}

void UsbTransferFunction::OnTransferInCompleted(
    UsbTransferStatus status,
    base::span<const uint8_t> data) {
  base::Value::Dict transfer_info;
  transfer_info.Set(kResultCodeKey, static_cast<int>(status));
  transfer_info.Set(kDataKey, base::Value(data));

  OnCompleted(status, std::move(transfer_info));
}

void UsbTransferFunction::OnTransferOutCompleted(UsbTransferStatus status) {
  base::Value::Dict transfer_info;
  transfer_info.Set(kResultCodeKey, static_cast<int>(status));
  transfer_info.Set(kDataKey, base::Value(base::Value::Type::BINARY));

  OnCompleted(status, std::move(transfer_info));
}

void UsbTransferFunction::OnDisconnect() {
  const auto status = UsbTransferStatus::DISCONNECT;
  base::Value::Dict transfer_info;
  transfer_info.Set(kResultCodeKey, static_cast<int>(status));
  OnCompleted(status, std::move(transfer_info));
}

UsbGenericTransferFunction::UsbGenericTransferFunction() = default;
UsbGenericTransferFunction::~UsbGenericTransferFunction() = default;

// const usb::InterruptTransfer::Params* or
// const usb::BulkTransfer::Params*
template <typename T>
ExtensionFunction::ResponseAction UsbGenericTransferFunction::DoTransfer(
    const T& params) {
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
  std::optional<usb::FindDevices::Params> parameters =
      FindDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  vendor_id_ = parameters->options.vendor_id;
  product_id_ = parameters->options.product_id;
  int interface_id = parameters->options.interface_id.value_or(
      UsbDevicePermissionData::SPECIAL_VALUE_ANY);
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
          mojom::APIPermissionID::kUsbDevice, param.get()) &&
      !IsUsbDeviceAllowedByPolicy(vendor_id_, product_id_)) {
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
            mojom::APIPermissionID::kUsbDevice, param.get()) &&
        !IsUsbDeviceAllowedByPolicy(vendor_id_, product_id_)) {
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
    device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_success() && device) {
    ApiResourceManager<UsbDeviceResource>* manager =
        ApiResourceManager<UsbDeviceResource>::Get(browser_context());
    UsbDeviceResource* resource =
        new UsbDeviceResource(extension_id(), guid, std::move(device));
    result_.Append(PopulateConnectionHandle(manager->Add(resource), vendor_id_,
                                            product_id_));
  }
  barrier_.Run();
}

void UsbFindDevicesFunction::OnDisconnect() {
  barrier_.Run();
}

void UsbFindDevicesFunction::OpenComplete() {
  Respond(WithArguments(std::move(result_)));
}

UsbGetDevicesFunction::UsbGetDevicesFunction() = default;
UsbGetDevicesFunction::~UsbGetDevicesFunction() = default;

ExtensionFunction::ResponseAction UsbGetDevicesFunction::Run() {
  std::optional<usb::GetDevices::Params> parameters =
      GetDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  base::Value::List result;
  for (const auto& device : devices) {
    if (device::UsbDeviceFilterMatchesAny(filters_, *device) &&
        HasDevicePermission(*device)) {
      Device api_device;
      usb_device_manager()->GetApiDevice(*device, &api_device);
      result.Append(api_device.ToValue());
    }
  }

  Respond(WithArguments(std::move(result)));
}

UsbGetUserSelectedDevicesFunction::UsbGetUserSelectedDevicesFunction() =
    default;
UsbGetUserSelectedDevicesFunction::~UsbGetUserSelectedDevicesFunction() =
    default;

ExtensionFunction::ResponseAction UsbGetUserSelectedDevicesFunction::Run() {
  std::optional<usb::GetUserSelectedDevices::Params> parameters =
      GetUserSelectedDevices::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (!user_gesture()) {
    return RespondNow(WithArguments(base::Value::List()));
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
      base::BindOnce(&UsbGetUserSelectedDevicesFunction::OnDevicesChosen,
                     this));
  return RespondLater();
}

void UsbGetUserSelectedDevicesFunction::OnDevicesChosen(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  base::Value::List result;
  auto* device_manager = usb_device_manager();
  DCHECK(device_manager);

  for (const auto& device : devices) {
    Device api_device;
    device_manager->GetApiDevice(*device, &api_device);
    result.Append(api_device.ToValue());
  }

  Respond(WithArguments(std::move(result)));
}

UsbGetConfigurationsFunction::UsbGetConfigurationsFunction() = default;
UsbGetConfigurationsFunction::~UsbGetConfigurationsFunction() = default;

ExtensionFunction::ResponseAction UsbGetConfigurationsFunction::Run() {
  std::optional<usb::GetConfigurations::Params> parameters =
      GetConfigurations::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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

  base::Value::List configs;
  uint8_t active_config_value = device_info->active_configuration;
  for (const auto& config : device_info->configurations) {
    DCHECK(config);
    ConfigDescriptor api_config = ConvertConfigDescriptor(*config);
    if (active_config_value &&
        config->configuration_value == active_config_value) {
      api_config.active = true;
    }
    configs.Append(api_config.ToValue());
  }
  return RespondNow(WithArguments(std::move(configs)));
}

UsbRequestAccessFunction::UsbRequestAccessFunction() = default;
UsbRequestAccessFunction::~UsbRequestAccessFunction() = default;

ExtensionFunction::ResponseAction UsbRequestAccessFunction::Run() {
  std::optional<usb::RequestAccess::Params> parameters =
      RequestAccess::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  return RespondNow(WithArguments(true));
}

UsbOpenDeviceFunction::UsbOpenDeviceFunction() = default;
UsbOpenDeviceFunction::~UsbOpenDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbOpenDeviceFunction::Run() {
  std::optional<usb::OpenDevice::Params> parameters =
      OpenDevice::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
    device::mojom::UsbOpenDeviceResultPtr result) {
  if (result->is_error() || !device) {
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
  Respond(WithArguments(PopulateConnectionHandle(manager->Add(resource),
                                                 device_info->vendor_id,
                                                 device_info->product_id)));
}

void UsbOpenDeviceFunction::OnDisconnect() {
  Respond(Error(kErrorDisconnect));
}

UsbSetConfigurationFunction::UsbSetConfigurationFunction() = default;
UsbSetConfigurationFunction::~UsbSetConfigurationFunction() = default;

ExtensionFunction::ResponseAction UsbSetConfigurationFunction::Run() {
  std::optional<usb::SetConfiguration::Params> parameters =
      SetConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  std::optional<usb::GetConfiguration::Params> parameters =
      GetConfiguration::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
        return RespondNow(WithArguments(api_config.ToValue()));
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
  std::optional<usb::ListInterfaces::Params> parameters =
      ListInterfaces::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
      base::Value::List result;
      for (const auto& interface : api_config.interfaces) {
        result.Append(interface.ToValue());
      }
      return RespondNow(WithArguments(std::move(result)));
    }
  }
  // Respond with an error if the config object can't be found according to
  // |active_config_value|.
  return RespondNow(Error(kErrorNotConfigured));
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() = default;
UsbCloseDeviceFunction::~UsbCloseDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbCloseDeviceFunction::Run() {
  std::optional<usb::CloseDevice::Params> parameters =
      CloseDevice::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  std::optional<usb::ClaimInterface::Params> parameters =
      ClaimInterface::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  device::mojom::UsbDevice* device = GetDeviceFromHandle(parameters->handle);
  if (!device) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device->ClaimInterface(
      parameters->interface_number,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UsbClaimInterfaceFunction::OnComplete, this),
          UsbClaimInterfaceResult::kFailure));
  return RespondLater();
}

void UsbClaimInterfaceFunction::OnComplete(UsbClaimInterfaceResult result) {
  if (result == UsbClaimInterfaceResult::kSuccess) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotClaimInterface));
  }
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() = default;
UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() = default;

ExtensionFunction::ResponseAction UsbReleaseInterfaceFunction::Run() {
  std::optional<usb::ReleaseInterface::Params> parameters =
      ReleaseInterface::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  std::optional<usb::SetInterfaceAlternateSetting::Params> parameters =
      SetInterfaceAlternateSetting::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  std::optional<usb::ControlTransfer::Params> parameters =
      ControlTransfer::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
  std::optional<usb::BulkTransfer::Params> parameters =
      BulkTransfer::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  return DoTransfer<const std::optional<usb::BulkTransfer::Params>>(parameters);
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() = default;
UsbInterruptTransferFunction::~UsbInterruptTransferFunction() = default;

ExtensionFunction::ResponseAction UsbInterruptTransferFunction::Run() {
  std::optional<usb::InterruptTransfer::Params> parameters =
      InterruptTransfer::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  return DoTransfer<const std::optional<usb::InterruptTransfer::Params>>(
      parameters);
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() = default;
UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() = default;

ExtensionFunction::ResponseAction UsbIsochronousTransferFunction::Run() {
  std::optional<usb::IsochronousTransfer::Params> parameters =
      IsochronousTransfer::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

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
    base::span<const uint8_t> data,
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

  base::Value::Dict transfer_info;
  transfer_info.Set(kResultCodeKey, base::Value(static_cast<int>(status)));
  transfer_info.Set(kDataKey, base::Value(std::move(buffer)));
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
  base::Value::Dict transfer_info;
  transfer_info.Set(kResultCodeKey, base::Value(static_cast<int>(status)));
  OnCompleted(status, std::move(transfer_info));
}

UsbResetDeviceFunction::UsbResetDeviceFunction() = default;
UsbResetDeviceFunction::~UsbResetDeviceFunction() = default;

ExtensionFunction::ResponseAction UsbResetDeviceFunction::Run() {
  parameters_ = ResetDevice::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters_);

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
    Respond(WithArguments(true));
  } else {
    ReleaseDeviceResource(parameters_->handle);

    base::Value::List error_args;
    error_args.Append(false);
    // Using ErrorWithArguments is discouraged but required to maintain
    // compatibility with existing applications.
    Respond(ErrorWithArguments(std::move(error_args), kErrorResetDevice));
  }
}

}  // namespace extensions
