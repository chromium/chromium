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
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "device/base/device_client.h"
#include "device/usb/public/cpp/usb_utils.h"
#include "device/usb/public/mojom/device_manager.mojom.h"
#include "device/usb/usb_descriptors.h"
#include "device/usb/usb_device_handle.h"
#include "device/usb/usb_service.h"
#include "extensions/browser/api/device_permissions_manager.h"
#include "extensions/browser/api/device_permissions_prompt.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/usb/usb_device_resource.h"
#include "extensions/browser/api/usb/usb_guid_map.h"
#include "extensions/browser/extension_function_constants.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/usb.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/usb_device_permission.h"

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
using device::mojom::UsbDeviceFilterPtr;
using device::UsbConfigDescriptor;
using device::UsbControlTransferRecipient;
using device::UsbControlTransferType;
using device::UsbDevice;
using device::UsbDeviceHandle;
using device::UsbEndpointDescriptor;
using device::UsbInterfaceDescriptor;
using device::UsbService;
using device::UsbSynchronizationType;
using device::UsbTransferDirection;
using device::UsbTransferStatus;
using device::UsbTransferType;
using device::UsbUsageType;
using std::string;
using std::vector;
using usb::ConfigDescriptor;
using usb::ControlTransferInfo;
using usb::ConnectionHandle;
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
bool GetTransferSize(const T& input, size_t* output) {
  if (input.direction == usb::DIRECTION_IN) {
    const int* length = input.length.get();
    if (length && *length >= 0 &&
        static_cast<size_t>(*length) < kMaxTransferLength) {
      *output = *length;
      return true;
    }
  } else if (input.direction == usb::DIRECTION_OUT) {
    if (input.data.get()) {
      *output = input.data->size();
      return true;
    }
  }
  return false;
}

template <class T>
scoped_refptr<base::RefCountedBytes> CreateBufferForTransfer(
    const T& input,
    UsbTransferDirection direction,
    size_t size) {
  if (size >= kMaxTransferLength)
    return nullptr;

  if (direction == UsbTransferDirection::INBOUND) {
    return base::MakeRefCounted<base::RefCountedBytes>(size);
  }

  if (direction == UsbTransferDirection::OUTBOUND && input.data &&
      size <= input.data->size()) {
    return base::MakeRefCounted<base::RefCountedBytes>(
        reinterpret_cast<const uint8_t*>(input.data->data()), size);
  }

  NOTREACHED();
  return nullptr;
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
    case device::USB_SYNCHRONIZATION_NONE:
      return usb::SYNCHRONIZATION_TYPE_NONE;
    case device::USB_SYNCHRONIZATION_ASYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_ASYNCHRONOUS;
    case device::USB_SYNCHRONIZATION_ADAPTIVE:
      return usb::SYNCHRONIZATION_TYPE_ADAPTIVE;
    case device::USB_SYNCHRONIZATION_SYNCHRONOUS:
      return usb::SYNCHRONIZATION_TYPE_SYNCHRONOUS;
    default:
      NOTREACHED();
      return usb::SYNCHRONIZATION_TYPE_NONE;
  }
}

UsageType ConvertUsageTypeToApi(const UsbUsageType& input) {
  switch (input) {
    case device::USB_USAGE_DATA:
      return usb::USAGE_TYPE_DATA;
    case device::USB_USAGE_FEEDBACK:
      return usb::USAGE_TYPE_FEEDBACK;
    case device::USB_USAGE_EXPLICIT_FEEDBACK:
      return usb::USAGE_TYPE_EXPLICITFEEDBACK;
    case device::USB_USAGE_PERIODIC:
      return usb::USAGE_TYPE_PERIODIC;
    case device::USB_USAGE_NOTIFICATION:
      return usb::USAGE_TYPE_NOTIFICATION;
    case device::USB_USAGE_RESERVED:
      return usb::USAGE_TYPE_NONE;
    default:
      NOTREACHED();
      return usb::USAGE_TYPE_NONE;
  }
}

EndpointDescriptor ConvertEndpointDescriptor(
    const UsbEndpointDescriptor& input) {
  EndpointDescriptor output;
  output.address = input.address;
  output.type = ConvertTransferTypeToApi(input.transfer_type);
  output.direction = ConvertDirectionToApi(input.direction);
  output.maximum_packet_size = input.maximum_packet_size;
  output.synchronization =
      ConvertSynchronizationTypeToApi(input.synchronization_type);
  output.usage = ConvertUsageTypeToApi(input.usage_type);
  output.polling_interval.reset(new int(input.polling_interval));
  output.extra_data.assign(input.extra_data.begin(), input.extra_data.end());
  return output;
}

InterfaceDescriptor ConvertInterfaceDescriptor(
    const UsbInterfaceDescriptor& input) {
  InterfaceDescriptor output;
  output.interface_number = input.interface_number;
  output.alternate_setting = input.alternate_setting;
  output.interface_class = input.interface_class;
  output.interface_subclass = input.interface_subclass;
  output.interface_protocol = input.interface_protocol;
  for (const UsbEndpointDescriptor& input_endpoint : input.endpoints)
    output.endpoints.push_back(ConvertEndpointDescriptor(input_endpoint));
  output.extra_data.assign(input.extra_data.begin(), input.extra_data.end());
  return output;
}

ConfigDescriptor ConvertConfigDescriptor(const UsbConfigDescriptor& input) {
  ConfigDescriptor output;
  output.configuration_value = input.configuration_value;
  output.self_powered = input.self_powered;
  output.remote_wakeup = input.remote_wakeup;
  output.max_power = input.maximum_power;
  for (const UsbInterfaceDescriptor& input_interface : input.interfaces)
    output.interfaces.push_back(ConvertInterfaceDescriptor(input_interface));
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

UsbPermissionCheckingFunction::UsbPermissionCheckingFunction()
    : device_permissions_manager_(nullptr) {
}

UsbPermissionCheckingFunction::~UsbPermissionCheckingFunction() {
}

bool UsbPermissionCheckingFunction::HasDevicePermission(
    scoped_refptr<UsbDevice> device) {
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
      UsbDevicePermission::CheckParam::ForUsbDevice(extension(), device.get());
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

UsbConnectionFunction::UsbConnectionFunction() {
}

UsbConnectionFunction::~UsbConnectionFunction() {
}

scoped_refptr<device::UsbDeviceHandle> UsbConnectionFunction::GetDeviceHandle(
    const extensions::api::usb::ConnectionHandle& handle) {
  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  if (!manager) {
    return nullptr;
  }

  UsbDeviceResource* resource = manager->Get(extension_id(), handle.handle);
  if (!resource) {
    return nullptr;
  }

  return resource->device();
}

void UsbConnectionFunction::ReleaseDeviceHandle(
    const extensions::api::usb::ConnectionHandle& handle) {
  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  manager->Remove(extension_id(), handle.handle);
}

UsbTransferFunction::UsbTransferFunction() {
}

UsbTransferFunction::~UsbTransferFunction() {
}

void UsbTransferFunction::OnCompleted(UsbTransferStatus status,
                                      scoped_refptr<base::RefCountedBytes> data,
                                      size_t length) {
  std::unique_ptr<base::DictionaryValue> transfer_info(
      new base::DictionaryValue());
  transfer_info->SetInteger(kResultCodeKey, static_cast<int>(status));

  if (data) {
    transfer_info->Set(kDataKey, base::Value::CreateWithCopiedBuffer(
                                     data->front_as<char>(), length));
  } else {
    transfer_info->Set(
        kDataKey, std::make_unique<base::Value>(base::Value::Type::BINARY));
  }

  if (status == UsbTransferStatus::COMPLETED) {
    Respond(OneArgument(std::move(transfer_info)));
  } else {
    std::unique_ptr<base::ListValue> error_args(new base::ListValue());
    error_args->Append(std::move(transfer_info));
    // Using ErrorWithArguments is discouraged but required to provide the
    // detailed transfer info as the transfer may have partially succeeded.
    Respond(ErrorWithArguments(std::move(error_args),
                               ConvertTransferStatusToApi(status)));
  }
}

UsbFindDevicesFunction::UsbFindDevicesFunction() {
}

UsbFindDevicesFunction::~UsbFindDevicesFunction() {
}

ExtensionFunction::ResponseAction UsbFindDevicesFunction::Run() {
  std::unique_ptr<extensions::api::usb::FindDevices::Params> parameters =
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

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    return RespondNow(Error(kErrorInitService));
  }

  service->GetDevices(
      base::Bind(&UsbFindDevicesFunction::OnGetDevicesComplete, this));
  return RespondLater();
}

void UsbFindDevicesFunction::OnGetDevicesComplete(
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  result_.reset(new base::ListValue());
  barrier_ = base::BarrierClosure(
      devices.size(), base::Bind(&UsbFindDevicesFunction::OpenComplete, this));

  for (const scoped_refptr<UsbDevice>& device : devices) {
    // Skip the device whose vendor and product ID do not match the target one.
    if (device->vendor_id() != vendor_id_ ||
        device->product_id() != product_id_) {
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
                                                      device.get());
    if (!extension()->permissions_data()->CheckAPIPermissionWithParam(
            APIPermission::kUsbDevice, param.get())) {
      barrier_.Run();
    } else {
      device->Open(base::Bind(&UsbFindDevicesFunction::OnDeviceOpened, this));
    }
  }
}

void UsbFindDevicesFunction::OnDeviceOpened(
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (device_handle.get()) {
    ApiResourceManager<UsbDeviceResource>* manager =
        ApiResourceManager<UsbDeviceResource>::Get(browser_context());
    UsbDeviceResource* resource =
        new UsbDeviceResource(extension_id(), device_handle);
    scoped_refptr<UsbDevice> device = device_handle->GetDevice();
    result_->Append(PopulateConnectionHandle(
        manager->Add(resource), device->vendor_id(), device->product_id()));
  }
  barrier_.Run();
}

void UsbFindDevicesFunction::OpenComplete() {
  Respond(OneArgument(std::move(result_)));
}

UsbGetDevicesFunction::UsbGetDevicesFunction() {
}

UsbGetDevicesFunction::~UsbGetDevicesFunction() {
}

ExtensionFunction::ResponseAction UsbGetDevicesFunction::Run() {
  std::unique_ptr<extensions::api::usb::GetDevices::Params> parameters =
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

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    return RespondNow(Error(kErrorInitService));
  }

  service->GetDevices(
      base::Bind(&UsbGetDevicesFunction::OnGetDevicesComplete, this));
  return RespondLater();
}

void UsbGetDevicesFunction::OnGetDevicesComplete(
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  UsbGuidMap* guid_map = UsbGuidMap::Get(browser_context());
  for (const scoped_refptr<UsbDevice>& device : devices) {
    if (UsbDeviceFilterMatchesAny(filters_, *device) &&
        HasDevicePermission(device)) {
      Device api_device;
      guid_map->GetApiDevice(device, &api_device);
      result->Append(api_device.ToValue());
    }
  }

  Respond(OneArgument(std::move(result)));
}

UsbGetUserSelectedDevicesFunction::UsbGetUserSelectedDevicesFunction() {
}

UsbGetUserSelectedDevicesFunction::~UsbGetUserSelectedDevicesFunction() {
}

ExtensionFunction::ResponseAction UsbGetUserSelectedDevicesFunction::Run() {
  std::unique_ptr<extensions::api::usb::GetUserSelectedDevices::Params>
      parameters = GetUserSelectedDevices::Params::Create(*args_);
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
    const std::vector<scoped_refptr<UsbDevice>>& devices) {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  UsbGuidMap* guid_map = UsbGuidMap::Get(browser_context());
  for (const auto& device : devices) {
    Device api_device;
    guid_map->GetApiDevice(device, &api_device);
    result->Append(api_device.ToValue());
  }

  Respond(OneArgument(std::move(result)));
}

UsbGetConfigurationsFunction::UsbGetConfigurationsFunction() {}

UsbGetConfigurationsFunction::~UsbGetConfigurationsFunction() {}

ExtensionFunction::ResponseAction UsbGetConfigurationsFunction::Run() {
  std::unique_ptr<extensions::api::usb::GetConfigurations::Params> parameters =
      GetConfigurations::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    return RespondNow(Error(kErrorInitService));
  }

  std::string guid;
  if (!UsbGuidMap::Get(browser_context())
           ->GetGuidFromId(parameters->device.device, &guid)) {
    return RespondNow(Error(kErrorNoDevice));
  }

  scoped_refptr<UsbDevice> device = service->GetDevice(guid);
  if (!device.get()) {
    return RespondNow(Error(kErrorNoDevice));
  }

  if (!HasDevicePermission(device)) {
    // This function must act as if there is no such device. Otherwise it can be
    // used to fingerprint unauthorized devices.
    return RespondNow(Error(kErrorNoDevice));
  }

  std::unique_ptr<base::ListValue> configs(new base::ListValue());
  const UsbConfigDescriptor* active_config = device->active_configuration();
  for (const UsbConfigDescriptor& config : device->configurations()) {
    ConfigDescriptor api_config = ConvertConfigDescriptor(config);
    if (active_config &&
        config.configuration_value == active_config->configuration_value) {
      api_config.active = true;
    }
    configs->Append(api_config.ToValue());
  }
  return RespondNow(OneArgument(std::move(configs)));
}

UsbRequestAccessFunction::UsbRequestAccessFunction() {
}

UsbRequestAccessFunction::~UsbRequestAccessFunction() {
}

ExtensionFunction::ResponseAction UsbRequestAccessFunction::Run() {
  std::unique_ptr<extensions::api::usb::RequestAccess::Params> parameters =
      RequestAccess::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());
  return RespondNow(OneArgument(std::make_unique<base::Value>(true)));
}

UsbOpenDeviceFunction::UsbOpenDeviceFunction() {
}

UsbOpenDeviceFunction::~UsbOpenDeviceFunction() {
}

ExtensionFunction::ResponseAction UsbOpenDeviceFunction::Run() {
  std::unique_ptr<extensions::api::usb::OpenDevice::Params> parameters =
      OpenDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  UsbService* service = device::DeviceClient::Get()->GetUsbService();
  if (!service) {
    return RespondNow(Error(kErrorInitService));
  }

  std::string guid;
  if (!UsbGuidMap::Get(browser_context())
           ->GetGuidFromId(parameters->device.device, &guid)) {
    return RespondNow(Error(kErrorNoDevice));
  }

  scoped_refptr<UsbDevice> device = service->GetDevice(guid);
  if (!device.get()) {
    return RespondNow(Error(kErrorNoDevice));
  }

  if (!HasDevicePermission(device)) {
    // This function must act as if there is no such device. Otherwise it can be
    // used to fingerprint unauthorized devices.
    return RespondNow(Error(kErrorNoDevice));
  }

  device->Open(base::Bind(&UsbOpenDeviceFunction::OnDeviceOpened, this));
  return RespondLater();
}

void UsbOpenDeviceFunction::OnDeviceOpened(
    scoped_refptr<UsbDeviceHandle> device_handle) {
  if (!device_handle.get()) {
    Respond(Error(kErrorOpen));
    return;
  }

  RecordDeviceLastUsed();

  ApiResourceManager<UsbDeviceResource>* manager =
      ApiResourceManager<UsbDeviceResource>::Get(browser_context());
  scoped_refptr<UsbDevice> device = device_handle->GetDevice();
  Respond(OneArgument(PopulateConnectionHandle(
      manager->Add(new UsbDeviceResource(extension_id(), device_handle)),
      device->vendor_id(), device->product_id())));
}

UsbSetConfigurationFunction::UsbSetConfigurationFunction() {
}

UsbSetConfigurationFunction::~UsbSetConfigurationFunction() {
}

ExtensionFunction::ResponseAction UsbSetConfigurationFunction::Run() {
  std::unique_ptr<extensions::api::usb::SetConfiguration::Params> parameters =
      SetConfiguration::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device_handle->SetConfiguration(
      parameters->configuration_value,
      base::Bind(&UsbSetConfigurationFunction::OnComplete, this));
  return RespondLater();
}

void UsbSetConfigurationFunction::OnComplete(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotSetConfiguration));
  }
}

UsbGetConfigurationFunction::UsbGetConfigurationFunction() {
}

UsbGetConfigurationFunction::~UsbGetConfigurationFunction() {
}

ExtensionFunction::ResponseAction UsbGetConfigurationFunction::Run() {
  std::unique_ptr<extensions::api::usb::GetConfiguration::Params> parameters =
      GetConfiguration::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const UsbConfigDescriptor* config_descriptor =
      device_handle->GetDevice()->active_configuration();
  if (config_descriptor) {
    ConfigDescriptor config = ConvertConfigDescriptor(*config_descriptor);
    config.active = true;
    return RespondNow(OneArgument(config.ToValue()));
  } else {
    return RespondNow(Error(kErrorNotConfigured));
  }
}

UsbListInterfacesFunction::UsbListInterfacesFunction() {
}

UsbListInterfacesFunction::~UsbListInterfacesFunction() {
}

ExtensionFunction::ResponseAction UsbListInterfacesFunction::Run() {
  std::unique_ptr<extensions::api::usb::ListInterfaces::Params> parameters =
      ListInterfaces::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const UsbConfigDescriptor* config_descriptor =
      device_handle->GetDevice()->active_configuration();
  if (config_descriptor) {
    ConfigDescriptor config = ConvertConfigDescriptor(*config_descriptor);

    std::unique_ptr<base::ListValue> result(new base::ListValue);
    for (size_t i = 0; i < config.interfaces.size(); ++i) {
      result->Append(config.interfaces[i].ToValue());
    }

    return RespondNow(OneArgument(std::move(result)));
  } else {
    return RespondNow(Error(kErrorNotConfigured));
  }
}

UsbCloseDeviceFunction::UsbCloseDeviceFunction() {
}

UsbCloseDeviceFunction::~UsbCloseDeviceFunction() {
}

ExtensionFunction::ResponseAction UsbCloseDeviceFunction::Run() {
  std::unique_ptr<extensions::api::usb::CloseDevice::Params> parameters =
      CloseDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  // The device handle is closed when the resource is destroyed.
  ReleaseDeviceHandle(parameters->handle);
  return RespondNow(NoArguments());
}

UsbClaimInterfaceFunction::UsbClaimInterfaceFunction() {
}

UsbClaimInterfaceFunction::~UsbClaimInterfaceFunction() {
}

ExtensionFunction::ResponseAction UsbClaimInterfaceFunction::Run() {
  std::unique_ptr<extensions::api::usb::ClaimInterface::Params> parameters =
      ClaimInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device_handle->ClaimInterface(
      parameters->interface_number,
      base::Bind(&UsbClaimInterfaceFunction::OnComplete, this));
  return RespondLater();
}

void UsbClaimInterfaceFunction::OnComplete(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotClaimInterface));
  }
}

UsbReleaseInterfaceFunction::UsbReleaseInterfaceFunction() {
}

UsbReleaseInterfaceFunction::~UsbReleaseInterfaceFunction() {
}

ExtensionFunction::ResponseAction UsbReleaseInterfaceFunction::Run() {
  std::unique_ptr<extensions::api::usb::ReleaseInterface::Params> parameters =
      ReleaseInterface::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device_handle->ReleaseInterface(
      parameters->interface_number,
      base::Bind(&UsbReleaseInterfaceFunction::OnComplete, this));
  return RespondLater();
}

void UsbReleaseInterfaceFunction::OnComplete(bool success) {
  if (success)
    Respond(NoArguments());
  else
    Respond(Error(kErrorCannotReleaseInterface));
}

UsbSetInterfaceAlternateSettingFunction::
    UsbSetInterfaceAlternateSettingFunction() {
}

UsbSetInterfaceAlternateSettingFunction::
    ~UsbSetInterfaceAlternateSettingFunction() {
}

ExtensionFunction::ResponseAction
UsbSetInterfaceAlternateSettingFunction::Run() {
  std::unique_ptr<extensions::api::usb::SetInterfaceAlternateSetting::Params>
      parameters = SetInterfaceAlternateSetting::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device_handle->SetInterfaceAlternateSetting(
      parameters->interface_number, parameters->alternate_setting,
      base::Bind(&UsbSetInterfaceAlternateSettingFunction::OnComplete, this));
  return RespondLater();
}

void UsbSetInterfaceAlternateSettingFunction::OnComplete(bool success) {
  if (success) {
    Respond(NoArguments());
  } else {
    Respond(Error(kErrorCannotSetInterfaceAlternateSetting));
  }
}

UsbControlTransferFunction::UsbControlTransferFunction() {
}

UsbControlTransferFunction::~UsbControlTransferFunction() {
}

ExtensionFunction::ResponseAction UsbControlTransferFunction::Run() {
  std::unique_ptr<extensions::api::usb::ControlTransfer::Params> parameters =
      ControlTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const ControlTransferInfo& transfer = parameters->transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;
  UsbControlTransferType request_type;
  UsbControlTransferRecipient recipient;
  size_t size = 0;

  if (!ConvertDirectionFromApi(transfer.direction, &direction)) {
    return RespondNow(Error(kErrorConvertDirection));
  }

  if (!ConvertRequestTypeFromApi(transfer.request_type, &request_type)) {
    return RespondNow(Error(kErrorConvertRequestType));
  }

  if (!ConvertRecipientFromApi(transfer.recipient, &recipient)) {
    return RespondNow(Error(kErrorConvertRecipient));
  }

  if (!GetTransferSize(transfer, &size)) {
    return RespondNow(Error(kErrorInvalidTransferLength));
  }

  scoped_refptr<base::RefCountedBytes> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer)
    return RespondNow(Error(kErrorMalformedParameters));

  int timeout = transfer.timeout ? *transfer.timeout : 0;
  if (timeout < 0) {
    return RespondNow(Error(kErrorInvalidTimeout));
  }

  device_handle->ControlTransfer(
      direction, request_type, recipient, transfer.request, transfer.value,
      transfer.index, buffer, timeout,
      base::Bind(&UsbControlTransferFunction::OnCompleted, this));
  return RespondLater();
}

UsbBulkTransferFunction::UsbBulkTransferFunction() {
}

UsbBulkTransferFunction::~UsbBulkTransferFunction() {
}

ExtensionFunction::ResponseAction UsbBulkTransferFunction::Run() {
  std::unique_ptr<extensions::api::usb::BulkTransfer::Params> parameters =
      BulkTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const GenericTransferInfo& transfer = parameters->transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;
  size_t size = 0;

  if (!ConvertDirectionFromApi(transfer.direction, &direction)) {
    return RespondNow(Error(kErrorConvertDirection));
  }

  if (!GetTransferSize(transfer, &size)) {
    return RespondNow(Error(kErrorInvalidTransferLength));
  }

  scoped_refptr<base::RefCountedBytes> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer)
    return RespondNow(Error(kErrorMalformedParameters));

  int timeout = transfer.timeout ? *transfer.timeout : 0;
  if (timeout < 0) {
    return RespondNow(Error(kErrorInvalidTimeout));
  }

  device_handle->GenericTransfer(
      direction, transfer.endpoint, buffer, timeout,
      base::Bind(&UsbBulkTransferFunction::OnCompleted, this));
  return RespondLater();
}

UsbInterruptTransferFunction::UsbInterruptTransferFunction() {
}

UsbInterruptTransferFunction::~UsbInterruptTransferFunction() {
}

ExtensionFunction::ResponseAction UsbInterruptTransferFunction::Run() {
  std::unique_ptr<extensions::api::usb::InterruptTransfer::Params> parameters =
      InterruptTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const GenericTransferInfo& transfer = parameters->transfer_info;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;
  size_t size = 0;

  if (!ConvertDirectionFromApi(transfer.direction, &direction)) {
    return RespondNow(Error(kErrorConvertDirection));
  }

  if (!GetTransferSize(transfer, &size)) {
    return RespondNow(Error(kErrorInvalidTransferLength));
  }

  scoped_refptr<base::RefCountedBytes> buffer =
      CreateBufferForTransfer(transfer, direction, size);
  if (!buffer)
    return RespondNow(Error(kErrorMalformedParameters));

  int timeout = transfer.timeout ? *transfer.timeout : 0;
  if (timeout < 0) {
    return RespondNow(Error(kErrorInvalidTimeout));
  }

  device_handle->GenericTransfer(
      direction, transfer.endpoint, buffer, timeout,
      base::Bind(&UsbInterruptTransferFunction::OnCompleted, this));
  return RespondLater();
}

UsbIsochronousTransferFunction::UsbIsochronousTransferFunction() {
}

UsbIsochronousTransferFunction::~UsbIsochronousTransferFunction() {
}

ExtensionFunction::ResponseAction UsbIsochronousTransferFunction::Run() {
  std::unique_ptr<extensions::api::usb::IsochronousTransfer::Params>
      parameters = IsochronousTransfer::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  const IsochronousTransferInfo& transfer = parameters->transfer_info;
  const GenericTransferInfo& generic_transfer = transfer.transfer_info;
  size_t size = 0;
  UsbTransferDirection direction = UsbTransferDirection::INBOUND;

  if (!ConvertDirectionFromApi(generic_transfer.direction, &direction))
    return RespondNow(Error(kErrorConvertDirection));

  if (!GetTransferSize(generic_transfer, &size))
    return RespondNow(Error(kErrorInvalidTransferLength));

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
    device_handle->IsochronousTransferIn(
        generic_transfer.endpoint, packet_lengths, timeout,
        base::Bind(&UsbIsochronousTransferFunction::OnCompleted, this));
  } else {
    scoped_refptr<base::RefCountedBytes> buffer = CreateBufferForTransfer(
        generic_transfer, direction, transfer.packets * transfer.packet_length);
    if (!buffer)
      return RespondNow(Error(kErrorMalformedParameters));

    device_handle->IsochronousTransferOut(
        generic_transfer.endpoint, buffer.get(), packet_lengths, timeout,
        base::Bind(&UsbIsochronousTransferFunction::OnCompleted, this));
  }
  return RespondLater();
}

void UsbIsochronousTransferFunction::OnCompleted(
    scoped_refptr<base::RefCountedBytes> data,
    const std::vector<UsbDeviceHandle::IsochronousPacket>& packets) {
  size_t length = std::accumulate(
      packets.begin(), packets.end(), 0,
      [](const size_t& a, const UsbDeviceHandle::IsochronousPacket& packet) {
        return a + packet.transferred_length;
      });
  std::vector<char> buffer;
  buffer.reserve(length);

  UsbTransferStatus status = UsbTransferStatus::COMPLETED;
  const char* data_ptr = data ? data->front_as<char>() : nullptr;
  for (const auto& packet : packets) {
    // Capture the error status of the first unsuccessful packet.
    if (status == UsbTransferStatus::COMPLETED &&
        packet.status != UsbTransferStatus::COMPLETED) {
      status = packet.status;
    }

    if (data) {
      buffer.insert(buffer.end(), data_ptr,
                    data_ptr + packet.transferred_length);
      data_ptr += packet.length;
    }
  }

  base::Value transfer_info(base::Value::Type::DICTIONARY);
  transfer_info.SetKey(kResultCodeKey, base::Value(static_cast<int>(status)));
  transfer_info.SetKey(kDataKey, base::Value(std::move(buffer)));
  if (status == UsbTransferStatus::COMPLETED) {
    Respond(
        OneArgument(base::Value::ToUniquePtrValue(std::move(transfer_info))));
  } else {
    auto error_args = std::make_unique<base::ListValue>();
    error_args->GetList().push_back(std::move(transfer_info));
    // Using ErrorWithArguments is discouraged but required to provide the
    // detailed transfer info as the transfer may have partially succeeded.
    Respond(ErrorWithArguments(std::move(error_args),
                               ConvertTransferStatusToApi(status)));
  }
}

UsbResetDeviceFunction::UsbResetDeviceFunction() {
}

UsbResetDeviceFunction::~UsbResetDeviceFunction() {
}

ExtensionFunction::ResponseAction UsbResetDeviceFunction::Run() {
  parameters_ = ResetDevice::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters_.get());

  scoped_refptr<UsbDeviceHandle> device_handle =
      GetDeviceHandle(parameters_->handle);
  if (!device_handle.get()) {
    return RespondNow(Error(kErrorNoConnection));
  }

  device_handle->ResetDevice(
      base::Bind(&UsbResetDeviceFunction::OnComplete, this));
  return RespondLater();
}

void UsbResetDeviceFunction::OnComplete(bool success) {
  if (success) {
    Respond(OneArgument(std::make_unique<base::Value>(true)));
  } else {
    scoped_refptr<UsbDeviceHandle> device_handle =
        GetDeviceHandle(parameters_->handle);
    if (device_handle.get()) {
      device_handle->Close();
    }
    ReleaseDeviceHandle(parameters_->handle);

    std::unique_ptr<base::ListValue> error_args(new base::ListValue());
    error_args->AppendBoolean(false);
    // Using ErrorWithArguments is discouraged but required to maintain
    // compatibility with existing applications.
    Respond(ErrorWithArguments(std::move(error_args), kErrorResetDevice));
  }
}

}  // namespace extensions
