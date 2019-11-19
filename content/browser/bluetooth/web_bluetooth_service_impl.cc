// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ID Not In Map Note: A service, characteristic, or descriptor ID not in the
// corresponding WebBluetoothServiceImpl map [service_id_to_device_address_,
// characteristic_id_to_service_id_, descriptor_id_to_characteristic_id_]
// implies a hostile renderer because a renderer obtains the corresponding ID
// from this class and it will be added to the map at that time.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/bluetooth_util.h"
#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "device/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using device::BluetoothAdapterFactoryWrapper;
using device::BluetoothUUID;

namespace content {

namespace {

blink::mojom::WebBluetoothResult TranslateConnectErrorAndRecord(
    device::BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case device::BluetoothDevice::ERROR_UNKNOWN:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNKNOWN);
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_ERROR;
    case device::BluetoothDevice::ERROR_INPROGRESS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::IN_PROGRESS);
      return blink::mojom::WebBluetoothResult::CONNECT_ALREADY_IN_PROGRESS;
    case device::BluetoothDevice::ERROR_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::FAILED);
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
    case device::BluetoothDevice::ERROR_AUTH_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_FAILED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_FAILED;
    case device::BluetoothDevice::ERROR_AUTH_CANCELED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_CANCELED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_CANCELED;
    case device::BluetoothDevice::ERROR_AUTH_REJECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_REJECTED);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_REJECTED;
    case device::BluetoothDevice::ERROR_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::AUTH_TIMEOUT);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_TIMEOUT;
    case device::BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::UNSUPPORTED_DEVICE);
      return blink::mojom::WebBluetoothResult::CONNECT_UNSUPPORTED_DEVICE;
    case device::BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED();
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
  }
  NOTREACHED();
  return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
}

blink::mojom::WebBluetoothResult TranslateGATTErrorAndRecord(
    device::BluetoothRemoteGattService::GattErrorCode error_code,
    UMAGATTOperation operation) {
  switch (error_code) {
    case device::BluetoothRemoteGattService::GATT_ERROR_UNKNOWN:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::UNKNOWN);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_ERROR;
    case device::BluetoothRemoteGattService::GATT_ERROR_FAILED:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::FAILED);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_FAILURE;
    case device::BluetoothRemoteGattService::GATT_ERROR_IN_PROGRESS:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::IN_PROGRESS);
      return blink::mojom::WebBluetoothResult::GATT_OPERATION_IN_PROGRESS;
    case device::BluetoothRemoteGattService::GATT_ERROR_INVALID_LENGTH:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::INVALID_LENGTH);
      return blink::mojom::WebBluetoothResult::GATT_INVALID_ATTRIBUTE_LENGTH;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_PERMITTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PERMITTED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PERMITTED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_AUTHORIZED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_AUTHORIZED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_PAIRED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_PAIRED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PAIRED;
    case device::BluetoothRemoteGattService::GATT_ERROR_NOT_SUPPORTED:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::NOT_SUPPORTED);
      return blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED;
  }
  NOTREACHED();
  return blink::mojom::WebBluetoothResult::GATT_UNTRANSLATED_ERROR_CODE;
}

// Max length of device name in filter. Bluetooth 5.0 3.C.3.2.2.3 states that
// the maximum device name length is 248 bytes (UTF-8 encoded).
constexpr size_t kMaxLengthForDeviceName = 248;

bool IsEmptyOrInvalidFilter(
    const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  // At least one member needs to be present.
  if (!filter->name && !filter->name_prefix && !filter->services)
    return true;

  // The renderer will never send a |name| or a |name_prefix| longer than
  // kMaxLengthForDeviceName.
  if (filter->name && filter->name->size() > kMaxLengthForDeviceName)
    return true;

  if (filter->name_prefix &&
      filter->name_prefix->size() > kMaxLengthForDeviceName)
    return true;

  // The |name_prefix| should not be empty
  if (filter->name_prefix && filter->name_prefix->empty())
    return true;

  return false;
}

bool IsRequestDeviceOptionsInvalid(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  if (options->accept_all_devices)
    return options->filters.has_value();

  return HasEmptyOrInvalidFilter(options->filters);
}

bool IsRequestScanOptionsInvalid(
    const blink::mojom::WebBluetoothRequestLEScanOptionsPtr& options) {
  if (options->accept_all_advertisements)
    return options->filters.has_value();

  return HasEmptyOrInvalidFilter(options->filters);
}

}  // namespace

bool HasEmptyOrInvalidFilter(
    const base::Optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  if (!filters) {
    return true;
  }

  return filters->empty()
             ? true
             : filters->end() != std::find_if(filters->begin(), filters->end(),
                                              IsEmptyOrInvalidFilter);
}

// Struct that holds the result of a cache query.
struct CacheQueryResult {
  CacheQueryResult() : outcome(CacheQueryOutcome::SUCCESS) {}

  explicit CacheQueryResult(CacheQueryOutcome outcome) : outcome(outcome) {}

  ~CacheQueryResult() {}

  blink::mojom::WebBluetoothResult GetWebResult() const {
    switch (outcome) {
      case CacheQueryOutcome::SUCCESS:
      case CacheQueryOutcome::BAD_RENDERER:
        NOTREACHED();
        return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::NO_DEVICE:
        return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::NO_SERVICE:
        return blink::mojom::WebBluetoothResult::SERVICE_NO_LONGER_EXISTS;
      case CacheQueryOutcome::NO_CHARACTERISTIC:
        return blink::mojom::WebBluetoothResult::
            CHARACTERISTIC_NO_LONGER_EXISTS;
      case CacheQueryOutcome::NO_DESCRIPTOR:
        return blink::mojom::WebBluetoothResult::DESCRIPTOR_NO_LONGER_EXISTS;
    }
    NOTREACHED();
    return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
  }

  device::BluetoothDevice* device = nullptr;
  device::BluetoothRemoteGattService* service = nullptr;
  device::BluetoothRemoteGattCharacteristic* characteristic = nullptr;
  device::BluetoothRemoteGattDescriptor* descriptor = nullptr;
  CacheQueryOutcome outcome;
};

struct GATTNotifySessionAndCharacteristicClient {
  GATTNotifySessionAndCharacteristicClient(
      std::unique_ptr<device::BluetoothGattNotifySession> session,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client)
      : gatt_notify_session(std::move(session)),
        characteristic_client(std::move(client)) {}

  std::unique_ptr<device::BluetoothGattNotifySession> gatt_notify_session;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
      characteristic_client;
};

WebBluetoothServiceImpl::WebBluetoothServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      connected_devices_(new FrameConnectedBluetoothDevices(render_frame_host)),
      render_frame_host_(render_frame_host),
      receiver_(this, std::move(receiver)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents());
}

WebBluetoothServiceImpl::~WebBluetoothServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ClearState();
}

void WebBluetoothServiceImpl::SetClientConnectionErrorHandler(
    base::OnceClosure closure) {
  receiver_.set_disconnect_handler(std::move(closure));
}

blink::mojom::WebBluetoothResult
WebBluetoothServiceImpl::GetBluetoothAllowed() {
  const url::Origin& requesting_origin =
      render_frame_host_->GetLastCommittedOrigin();
  const url::Origin& embedding_origin =
      web_contents()->GetMainFrame()->GetLastCommittedOrigin();

  // TODO(crbug.com/518042): Enforce correctly-delegated permissions instead of
  // matching origins. When relaxing this, take care to handle non-sandboxed
  // unique origins.
  if (!embedding_origin.IsSameOriginWith(requesting_origin)) {
    return blink::mojom::WebBluetoothResult::
        REQUEST_DEVICE_FROM_CROSS_ORIGIN_IFRAME;
  }
  // IsSameOriginWith() no longer excludes opaque origins.
  // TODO(https://crbug.com/994454): Exclude opaque origins explicitly.

  // Some embedders that don't support Web Bluetooth indicate this by not
  // returning a chooser.
  // TODO(https://crbug.com/993829): Perform this check once there is a way to
  // check if a platform is capable of producing a chooser and return a
  // |blink::mojom::WebBluetoothResult::WEB_BLUETOOTH_NOT_SUPPORTED| error.
  switch (GetContentClient()->browser()->AllowWebBluetooth(
      web_contents()->GetBrowserContext(), requesting_origin,
      embedding_origin)) {
    case ContentBrowserClient::AllowWebBluetoothResult::BLOCK_POLICY:
      return blink::mojom::WebBluetoothResult::
          CHOOSER_NOT_SHOWN_API_LOCALLY_DISABLED;
    case ContentBrowserClient::AllowWebBluetoothResult::BLOCK_GLOBALLY_DISABLED:
      return blink::mojom::WebBluetoothResult::
          CHOOSER_NOT_SHOWN_API_GLOBALLY_DISABLED;
    case ContentBrowserClient::AllowWebBluetoothResult::ALLOW:
      return blink::mojom::WebBluetoothResult::SUCCESS;
  }
}

bool WebBluetoothServiceImpl::IsDevicePaired(
    const std::string& device_address) {
  return allowed_devices().GetDeviceId(device_address) != nullptr;
}

void WebBluetoothServiceImpl::OnBluetoothScanningPromptEvent(
    BluetoothScanningPrompt::Event event,
    BluetoothDeviceScanningPromptController* prompt_controller) {
  // It is possible for |scanning_clients_| to be empty if a Mojo connection
  // error has occurred before this method was called.
  if (scanning_clients_.empty())
    return;

  auto& client = scanning_clients_.back();

  DCHECK(client->prompt_controller() == prompt_controller);

  auto result = blink::mojom::WebBluetoothResult::SUCCESS;
  if (event == BluetoothScanningPrompt::Event::kAllow) {
    result = blink::mojom::WebBluetoothResult::SUCCESS;
    StoreAllowedScanOptions(client->scan_options());
  } else if (event == BluetoothScanningPrompt::Event::kBlock) {
    result = blink::mojom::WebBluetoothResult::SCANNING_BLOCKED;
    const url::Origin requesting_origin =
        render_frame_host_->GetLastCommittedOrigin();
    const url::Origin embedding_origin =
        web_contents()->GetMainFrame()->GetLastCommittedOrigin();
    GetContentClient()->browser()->BlockBluetoothScanning(
        web_contents()->GetBrowserContext(), requesting_origin,
        embedding_origin);
  } else if (event == BluetoothScanningPrompt::Event::kCanceled) {
    result = blink::mojom::WebBluetoothResult::PROMPT_CANCELED;
  } else {
    NOTREACHED();
  }

  client->RunRequestScanningStartCallback(std::move(result));
  client->set_prompt_controller(nullptr);
  if (event == BluetoothScanningPrompt::Event::kAllow) {
    client->set_allow_send_event(true);
  } else if (event == BluetoothScanningPrompt::Event::kBlock) {
    // Here because user explicitly blocks the permission to do Bluetooth
    // scanning in one request, it can be interpreted as user wants the current
    // and all previous scanning to be blocked, so remove all existing scanning
    // clients.
    scanning_clients_.clear();
    allowed_scan_filters_.clear();
    accept_all_advertisements_ = false;
  } else if (event == BluetoothScanningPrompt::Event::kCanceled) {
    scanning_clients_.pop_back();
  } else {
    NOTREACHED();
  }
}

WebBluetoothServiceImpl::ScanningClient::ScanningClient(
    mojo::AssociatedRemote<blink::mojom::WebBluetoothScanClient> client,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback,
    BluetoothDeviceScanningPromptController* prompt_controller)
    : client_(std::move(client)),
      options_(std::move(options)),
      callback_(std::move(callback)),
      prompt_controller_(prompt_controller) {
  DCHECK(options_->filters.has_value() || options_->accept_all_advertisements);
  client_.set_disconnect_handler(base::BindRepeating(
      &ScanningClient::DisconnectionHandler, base::Unretained(this)));
}

WebBluetoothServiceImpl::ScanningClient::~ScanningClient() {}

bool WebBluetoothServiceImpl::ScanningClient::SendEvent(
    blink::mojom::WebBluetoothScanResultPtr result) {
  if (disconnected_)
    return false;

  if (options_->accept_all_advertisements) {
    if (prompt_controller_)
      AddFilteredDeviceToPrompt(result->device->id.str(), result->name);

    if (allow_send_event_)
      client_->ScanEvent(std::move(result));

    return true;
  }

  DCHECK(options_->filters.has_value());

  // For every filter, we're going to check to see if a |name|, |name_prefix|,
  // or |services| have been set. If one of these is set, we will check the
  // scan result to see if it matches the filter's value.  If it doesn't, we'll
  // just continue with the next filter. If all of the properties in a filter
  // have a match, we can post the ScanEvent.  Otherwise, we are going to drop
  // it.  This logic can be reduced a bit, but I think clarity will decrease.

  for (auto& filter : options_->filters.value()) {
    // Check to see if there is a direct match against the advertisement name
    if (filter->name.has_value()) {
      if (!result->name.has_value())
        continue;

      if (filter->name.value() != result->name.value())
        continue;
    }

    // Check if there is a name prefix match
    if (filter->name_prefix.has_value()) {
      if (!result->name.has_value())
        continue;

      if (!base::StartsWith(result->name.value(), filter->name_prefix.value(),
                            base::CompareCase::SENSITIVE)) {
        continue;
      }
    }
    // Check to see if there is a service uuid match
    if (filter->services.has_value()) {
      bool found_uuid_match = false;
      for (auto& filter_uuid : filter->services.value()) {
        found_uuid_match = base::Contains(result->uuids, filter_uuid);
        if (found_uuid_match)
          break;
      }
      if (!found_uuid_match)
        continue;
    }
    // TODO(crbug.com/707635): Support manufacturerData and serviceData filters.

    if (prompt_controller_)
      AddFilteredDeviceToPrompt(result->device->id.str(), result->name);

    if (allow_send_event_)
      client_->ScanEvent(std::move(result));
    return true;
  }

  // Event was filtered out.
  return true;
}

void WebBluetoothServiceImpl::ScanningClient::RunRequestScanningStartCallback(
    blink::mojom::WebBluetoothResult result) {
  if (result == blink::mojom::WebBluetoothResult::SUCCESS) {
    auto scanning_result =
        blink::mojom::RequestScanningStartResult::NewOptions(options_.Clone());
    std::move(callback_).Run(std::move(scanning_result));
  } else if (result == blink::mojom::WebBluetoothResult::SCANNING_BLOCKED ||
             result == blink::mojom::WebBluetoothResult::PROMPT_CANCELED) {
    auto scanning_result =
        blink::mojom::RequestScanningStartResult::NewErrorResult(result);
    std::move(callback_).Run(std::move(scanning_result));
  } else {
    NOTREACHED();
  }
}

void WebBluetoothServiceImpl::ScanningClient::DisconnectionHandler() {
  disconnected_ = true;
}

void WebBluetoothServiceImpl::ScanningClient::AddFilteredDeviceToPrompt(
    const std::string& device_id,
    const base::Optional<std::string>& device_name) {
  bool should_update_name = device_name.has_value();
  base::string16 device_name_for_display =
      base::UTF8ToUTF16(device_name.value_or(""));
  prompt_controller_->AddFilteredDevice(device_id, should_update_name,
                                        device_name_for_display);
}

void WebBluetoothServiceImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (navigation_handle->HasCommitted() &&
      navigation_handle->GetRenderFrameHost() == render_frame_host_ &&
      !navigation_handle->IsSameDocument()) {
    ClearState();
  }
}

void WebBluetoothServiceImpl::OnVisibilityChanged(Visibility visibility) {
  if (visibility == content::Visibility::HIDDEN ||
      visibility == content::Visibility::OCCLUDED) {
    allowed_scan_filters_.clear();
    accept_all_advertisements_ = false;
    scanning_clients_.clear();
  }
}

void WebBluetoothServiceImpl::AdapterPoweredChanged(
    device::BluetoothAdapter* adapter,
    bool powered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AdapterPoweredChanged(powered);
  }
}

void WebBluetoothServiceImpl::DeviceAdded(device::BluetoothAdapter* adapter,
                                          device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }
}

void WebBluetoothServiceImpl::DeviceChanged(device::BluetoothAdapter* adapter,
                                            device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  if (!device->IsGattConnected()) {
    base::Optional<blink::WebBluetoothDeviceId> device_id =
        connected_devices_->CloseConnectionToDeviceWithAddress(
            device->GetAddress());

    // Since the device disconnected we need to send an error for pending
    // primary services requests.
    RunPendingPrimaryServicesRequests(device);
  }
}

void WebBluetoothServiceImpl::DeviceAdvertisementReceived(
    const std::string& device_address,
    const base::Optional<std::string>& device_name,
    const base::Optional<std::string>& advertisement_name,
    base::Optional<int8_t> rssi,
    base::Optional<int8_t> tx_power,
    base::Optional<uint16_t> appearance,
    const device::BluetoothDevice::UUIDList& advertised_uuids,
    const device::BluetoothDevice::ServiceDataMap& service_data_map,
    const device::BluetoothDevice::ManufacturerDataMap& manufacturer_data_map) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!discovery_session_ || !discovery_session_->IsActive())
    return;

  auto client = scanning_clients_.begin();
  while (client != scanning_clients_.end()) {
    auto device = blink::mojom::WebBluetoothDevice::New();
    device->id = allowed_devices().AddDevice(device_address);
    device->name = device_name;

    auto result = blink::mojom::WebBluetoothScanResult::New();
    result->device = std::move(device);

    result->name = advertisement_name;

    // Note about the default value for these optional types. On the other side
    // of this IPC, the receiver will be checking to see if |*_is_set| is true
    // before using the value. Here we chose reasonable defaults in case the
    // other side does something incorrect. We have to do this manual
    // serialization because mojo does not support optional primitive types.
    result->appearance_is_set = appearance.has_value();
    result->appearance = appearance.value_or(/*not present=*/0xffc0);

    result->rssi_is_set = rssi.has_value();
    result->rssi = rssi.value_or(/*invalid value=*/128);

    result->tx_power_is_set = tx_power.has_value();
    result->tx_power = tx_power.value_or(/*invalid value=*/128);

    std::vector<device::BluetoothUUID> uuids;
    for (auto& uuid : advertised_uuids)
      uuids.push_back(device::BluetoothUUID(uuid.canonical_value()));
    result->uuids = std::move(uuids);

    auto& manufacturer_data = result->manufacturer_data;
    for (auto& it : manufacturer_data_map)
      manufacturer_data.emplace(it.first, it.second);

    base::flat_map<std::string, std::vector<uint8_t>> services;
    for (auto& it : service_data_map)
      services[it.first.canonical_value()] = it.second;
    result->service_data = std::move(services);

    bool okay = (*client)->SendEvent(std::move(result));
    if (!okay) {
      client = scanning_clients_.erase(client);
      continue;
    }

    ++client;
  }

  // If we don't have any bound clients, clean things up.
  if (scanning_clients_.empty()) {
    discovery_session_->Stop(base::DoNothing(), base::DoNothing());
    discovery_session_ = nullptr;
    return;
  }
}

void WebBluetoothServiceImpl::GattServicesDiscovered(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(1) << "Services discovered for device: " << device->GetAddress();

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  RunPendingPrimaryServicesRequests(device);
}

void WebBluetoothServiceImpl::GattCharacteristicValueChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // Don't notify of characteristics that we haven't returned.
  if (!base::Contains(characteristic_id_to_service_id_,
                      characteristic->GetIdentifier())) {
    return;
  }

  // TODO(crbug.com/541390): Don't send notifications when they haven't been
  // requested by the client.

  // On Chrome OS and Linux, GattCharacteristicValueChanged is called before the
  // success callback for ReadRemoteCharacteristic is called, which could result
  // in an event being fired before the readValue promise is resolved.
  if (!base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &WebBluetoothServiceImpl::NotifyCharacteristicValueChanged,
              weak_ptr_factory_.GetWeakPtr(), characteristic->GetIdentifier(),
              value))) {
    LOG(WARNING) << "No TaskRunner.";
  }
}

void WebBluetoothServiceImpl::NotifyCharacteristicValueChanged(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value) {
  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (iter != characteristic_id_to_notify_session_.end()) {
    iter->second->characteristic_client->RemoteCharacteristicValueChanged(
        value);
  }
}

void WebBluetoothServiceImpl::GetAvailability(
    GetAvailabilityCallback callback) {
  if (GetBluetoothAllowed() != blink::mojom::WebBluetoothResult::SUCCESS) {
    std::move(callback).Run(/*result=*/false);
    return;
  }

  if (!BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
    std::move(callback).Run(/*result=*/false);
    return;
  }

  auto get_availability_impl = base::BindOnce(
      [](GetAvailabilityCallback callback,
         scoped_refptr<device::BluetoothAdapter> adapter) {
        std::move(callback).Run(adapter->IsPresent());
      },
      std::move(callback));

  auto* adapter = GetAdapter();
  if (adapter) {
    std::move(get_availability_impl).Run(adapter);
    return;
  }

  BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
      this, std::move(get_availability_impl));
}

void WebBluetoothServiceImpl::RequestDevice(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    RequestDeviceCallback callback) {
  RecordRequestDeviceOptions(options);

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this, base::BindOnce(&WebBluetoothServiceImpl::RequestDeviceImpl,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(options), std::move(callback)));
      return;
    }
    RecordRequestDeviceOutcome(
        UMARequestDeviceOutcome::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE,
        nullptr /* device */);
    return;
  }
  RequestDeviceImpl(std::move(options), std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::RemoteServerConnect(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothServerClient>
        client,
    RemoteServerConnectCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!allowed_devices().IsAllowedToGATTConnect(device_id)) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED);
    return;
  }

  const CacheQueryResult query_result = QueryCacheForDevice(device_id);

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordConnectGATTOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    DVLOG(1) << "Already connected.";
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  // It's possible for WebBluetoothServiceImpl to issue two successive
  // connection requests for which it would get two successive responses
  // and consequently try to insert two BluetoothGattConnections for the
  // same device. WebBluetoothServiceImpl should reject or queue connection
  // requests if there is a pending connection already, but the platform
  // abstraction doesn't currently support checking for pending connections.
  // TODO(ortuno): CHECK that this never happens once the platform
  // abstraction allows to check for pending connections.
  // http://crbug.com/583544
  const base::TimeTicks start_time = base::TimeTicks::Now();
  mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient>
      web_bluetooth_server_client(std::move(client));

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.device->CreateGattConnection(
      base::Bind(&WebBluetoothServiceImpl::OnCreateGATTConnectionSuccess,
                 weak_ptr_factory_.GetWeakPtr(), device_id, start_time,
                 base::Passed(&web_bluetooth_server_client), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnCreateGATTConnectionFailed,
                 weak_ptr_factory_.GetWeakPtr(), start_time,
                 copyable_callback));
}

void WebBluetoothServiceImpl::RemoteServerDisconnect(
    const blink::WebBluetoothDeviceId& device_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    DVLOG(1) << "Disconnecting device: " << device_id.str();
    connected_devices_->CloseConnectionToDeviceWithId(device_id);
  }
}

void WebBluetoothServiceImpl::RemoteServerGetPrimaryServices(
    const blink::WebBluetoothDeviceId& device_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& services_uuid,
    RemoteServerGetPrimaryServicesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordGetPrimaryServicesServices(quantity, services_uuid);

  if (!allowed_devices().IsAllowedToAccessAtLeastOneService(device_id)) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE,
        base::nullopt /* service */);
    return;
  }

  if (services_uuid && !allowed_devices().IsAllowedToAccessService(
                           device_id, services_uuid.value())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_SERVICE,
        base::nullopt /* service */);
    return;
  }

  const CacheQueryResult query_result = QueryCacheForDevice(device_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetPrimaryServicesOutcome(quantity, query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            base::nullopt /* service */);
    return;
  }

  const std::string& device_address = query_result.device->GetAddress();

  // We can't know if a service is present or not until GATT service discovery
  // is complete for the device.
  if (query_result.device->IsGattServicesDiscoveryComplete()) {
    RemoteServerGetPrimaryServicesImpl(device_id, quantity, services_uuid,
                                       std::move(callback),
                                       query_result.device);
    return;
  }

  DVLOG(1) << "Services not yet discovered.";
  pending_primary_services_requests_[device_address].push_back(base::BindOnce(
      &WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl,
      base::Unretained(this), device_id, quantity, services_uuid,
      std::move(callback)));
}

void WebBluetoothServiceImpl::RemoteServiceGetCharacteristics(
    const std::string& service_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& characteristics_uuid,
    RemoteServiceGetCharacteristicsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordGetCharacteristicsCharacteristic(quantity, characteristics_uuid);

  if (characteristics_uuid &&
      BluetoothBlocklist::Get().IsExcluded(characteristics_uuid.value())) {
    RecordGetCharacteristicsOutcome(quantity,
                                    UMAGetCharacteristicOutcome::BLOCKLISTED);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_CHARACTERISTIC_UUID,
        base::nullopt /* characteristics */);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForService(service_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetCharacteristicsOutcome(quantity, query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            base::nullopt /* characteristics */);
    return;
  }

  std::vector<device::BluetoothRemoteGattCharacteristic*> characteristics =
      characteristics_uuid ? query_result.service->GetCharacteristicsByUUID(
                                 characteristics_uuid.value())
                           : query_result.service->GetCharacteristics();

  std::vector<blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr>
      response_characteristics;
  for (device::BluetoothRemoteGattCharacteristic* characteristic :
       characteristics) {
    if (BluetoothBlocklist::Get().IsExcluded(characteristic->GetUUID())) {
      continue;
    }
    std::string characteristic_instance_id = characteristic->GetIdentifier();
    auto insert_result = characteristic_id_to_service_id_.insert(
        std::make_pair(characteristic_instance_id, service_instance_id));
    // If value is already in map, DCHECK it's valid.
    if (!insert_result.second)
      DCHECK(insert_result.first->second == service_instance_id);

    blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr characteristic_ptr =
        blink::mojom::WebBluetoothRemoteGATTCharacteristic::New();
    characteristic_ptr->instance_id = characteristic_instance_id;
    characteristic_ptr->uuid = characteristic->GetUUID();
    characteristic_ptr->properties =
        static_cast<uint32_t>(characteristic->GetProperties());
    response_characteristics.push_back(std::move(characteristic_ptr));

    if (quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
      break;
    }
  }

  if (!response_characteristics.empty()) {
    RecordGetCharacteristicsOutcome(quantity,
                                    UMAGetCharacteristicOutcome::SUCCESS);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_characteristics));
    return;
  }

  RecordGetCharacteristicsOutcome(
      quantity, characteristics_uuid
                    ? UMAGetCharacteristicOutcome::NOT_FOUND
                    : UMAGetCharacteristicOutcome::NO_CHARACTERISTICS);
  std::move(callback).Run(
      characteristics_uuid
          ? blink::mojom::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND
          : blink::mojom::WebBluetoothResult::NO_CHARACTERISTICS_FOUND,
      base::nullopt /* characteristics */);
}

void WebBluetoothServiceImpl::RemoteCharacteristicGetDescriptors(
    const std::string& characteristic_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& descriptors_uuid,
    RemoteCharacteristicGetDescriptorsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordGetDescriptorsDescriptor(quantity, descriptors_uuid);

  if (descriptors_uuid &&
      BluetoothBlocklist::Get().IsExcluded(descriptors_uuid.value())) {
    RecordGetDescriptorsOutcome(quantity, UMAGetDescriptorOutcome::BLOCKLISTED);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_DESCRIPTOR_UUID,
        base::nullopt /* descriptor */);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordGetDescriptorsOutcome(quantity, query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            base::nullopt /* descriptor */);
    return;
  }

  auto descriptors = descriptors_uuid
                         ? query_result.characteristic->GetDescriptorsByUUID(
                               descriptors_uuid.value())
                         : query_result.characteristic->GetDescriptors();

  std::vector<blink::mojom::WebBluetoothRemoteGATTDescriptorPtr>
      response_descriptors;
  for (device::BluetoothRemoteGattDescriptor* descriptor : descriptors) {
    if (BluetoothBlocklist::Get().IsExcluded(descriptor->GetUUID())) {
      continue;
    }
    std::string descriptor_instance_id = descriptor->GetIdentifier();
    auto insert_result = descriptor_id_to_characteristic_id_.insert(
        {descriptor_instance_id, characteristic_instance_id});
    // If value is already in map, DCHECK it's valid.
    if (!insert_result.second)
      DCHECK(insert_result.first->second == characteristic_instance_id);

    auto descriptor_ptr(blink::mojom::WebBluetoothRemoteGATTDescriptor::New());
    descriptor_ptr->instance_id = descriptor_instance_id;
    descriptor_ptr->uuid = descriptor->GetUUID();
    response_descriptors.push_back(std::move(descriptor_ptr));

    if (quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
      break;
    }
  }

  if (!response_descriptors.empty()) {
    RecordGetDescriptorsOutcome(quantity, UMAGetDescriptorOutcome::SUCCESS);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_descriptors));
    return;
  }
  RecordGetDescriptorsOutcome(
      quantity, descriptors_uuid ? UMAGetDescriptorOutcome::NOT_FOUND
                                 : UMAGetDescriptorOutcome::NO_DESCRIPTORS);
  std::move(callback).Run(
      descriptors_uuid ? blink::mojom::WebBluetoothResult::DESCRIPTOR_NOT_FOUND
                       : blink::mojom::WebBluetoothResult::NO_DESCRIPTORS_FOUND,
      base::nullopt /* descriptors */);
}

void WebBluetoothServiceImpl::RemoteCharacteristicReadValue(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicReadValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicReadValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            base::nullopt /* value */);
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromReads(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_READ,
                            base::nullopt /* value */);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = AdaptCallbackForRepeating(std::move(callback));
  query_result.characteristic->ReadRemoteCharacteristic(
      base::Bind(&WebBluetoothServiceImpl::OnCharacteristicReadValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnCharacteristicReadValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicWriteValue(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    RemoteCharacteristicWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We perform the length check on the renderer side. So if we
  // get a value with length > 512, we can assume it's a hostile
  // renderer and kill it.
  if (value.size() > 512) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordCharacteristicWriteValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromWrites(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_WRITE);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.characteristic->WriteRemoteCharacteristic(
      value,
      base::Bind(&WebBluetoothServiceImpl::OnCharacteristicWriteValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnCharacteristicWriteValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStartNotifications(
    const std::string& characteristic_instance_id,
    mojo::PendingAssociatedRemote<
        blink::mojom::WebBluetoothCharacteristicClient> client,
    RemoteCharacteristicStartNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (iter != characteristic_id_to_notify_session_.end() &&
      iter->second->gatt_notify_session->IsActive()) {
    // If the frame has already started notifications and the notifications
    // are active we return SUCCESS.
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordStartNotificationsOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  device::BluetoothRemoteGattCharacteristic::Properties notify_or_indicate =
      query_result.characteristic->GetProperties() &
      (device::BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
       device::BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE);
  if (!notify_or_indicate) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED);
    return;
  }

  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
      characteristic_client(std::move(client));

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.characteristic->StartNotifySession(
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionSuccess,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&characteristic_client), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnStartNotifySessionFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStopNotifications(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicStopNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  auto notify_session_iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (notify_session_iter == characteristic_id_to_notify_session_.end()) {
    // If the frame hasn't subscribed to notifications before we just
    // run the callback.
    std::move(callback).Run();
    return;
  }
  notify_session_iter->second->gatt_notify_session->Stop(
      base::Bind(&WebBluetoothServiceImpl::OnStopNotifySessionComplete,
                 weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
                 base::Passed(&callback)));
}

void WebBluetoothServiceImpl::RemoteDescriptorReadValue(
    const std::string& descriptor_instance_id,
    RemoteDescriptorReadValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForDescriptor(descriptor_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordDescriptorReadValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            base::nullopt /* value */);
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromReads(
          query_result.descriptor->GetUUID())) {
    RecordDescriptorReadValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_READ,
                            base::nullopt /* value */);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.descriptor->ReadRemoteDescriptor(
      base::Bind(&WebBluetoothServiceImpl::OnDescriptorReadValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnDescriptorReadValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteDescriptorWriteValue(
    const std::string& descriptor_instance_id,
    const std::vector<uint8_t>& value,
    RemoteDescriptorWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We perform the length check on the renderer side. So if we
  // get a value with length > 512, we can assume it's a hostile
  // renderer and kill it.
  if (value.size() > 512) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForDescriptor(descriptor_instance_id);

  if (query_result.outcome == CacheQueryOutcome::BAD_RENDERER) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::SUCCESS) {
    RecordDescriptorWriteValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromWrites(
          query_result.descriptor->GetUUID())) {
    RecordDescriptorWriteValueOutcome(UMAGATTOperationOutcome::BLOCKLISTED);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_WRITE);
    return;
  }

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.descriptor->WriteRemoteDescriptor(
      value,
      base::Bind(&WebBluetoothServiceImpl::OnDescriptorWriteValueSuccess,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnDescriptorWriteValueFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RequestScanningStart(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothScanClient>
        client_info,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  mojo::AssociatedRemote<blink::mojom::WebBluetoothScanClient> client(
      std::move(client_info));

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this,
          base::BindOnce(&WebBluetoothServiceImpl::RequestScanningStartImpl,
                         weak_ptr_factory_.GetWeakPtr(), std::move(client),
                         std::move(options), std::move(callback)));
      return;
    }
    auto result = blink::mojom::RequestScanningStartResult::NewErrorResult(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    std::move(callback).Run(std::move(result));
    return;
  }

  RequestScanningStartImpl(std::move(client), std::move(options),
                           std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::RequestScanningStartImpl(
    mojo::AssociatedRemote<blink::mojom::WebBluetoothScanClient> client,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The renderer should never send invalid options.
  if (IsRequestScanOptionsInvalid(options)) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  if (!adapter) {
    auto result = blink::mojom::RequestScanningStartResult::NewErrorResult(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    std::move(callback).Run(std::move(result));
    return;
  }

  const url::Origin requesting_origin =
      render_frame_host_->GetLastCommittedOrigin();
  const url::Origin embedding_origin =
      web_contents()->GetMainFrame()->GetLastCommittedOrigin();

  bool blocked = GetContentClient()->browser()->IsBluetoothScanningBlocked(
      web_contents()->GetBrowserContext(), requesting_origin, embedding_origin);

  if (blocked) {
    auto result = blink::mojom::RequestScanningStartResult::NewErrorResult(
        blink::mojom::WebBluetoothResult::SCANNING_BLOCKED);
    std::move(callback).Run(std::move(result));
    return;
  }

  if (discovery_callback_) {
    auto result = blink::mojom::RequestScanningStartResult::NewErrorResult(
        blink::mojom::WebBluetoothResult::PROMPT_CANCELED);
    std::move(callback).Run(std::move(result));
    return;
  }

  if (discovery_session_) {
    if (AreScanFiltersAllowed(options->filters)) {
      auto scanning_client = std::make_unique<ScanningClient>(
          std::move(client), std::move(options), std::move(callback), nullptr);
      scanning_client->RunRequestScanningStartCallback(
          blink::mojom::WebBluetoothResult::SUCCESS);
      scanning_client->set_allow_send_event(true);
      scanning_clients_.push_back(std::move(scanning_client));
      return;
    }

    // By resetting |device_scanning_prompt_controller_|, it returns an error if
    // there are duplicate calls to RequestScanningStart().
    device_scanning_prompt_controller_ =
        std::make_unique<BluetoothDeviceScanningPromptController>(
            this, render_frame_host_);

    scanning_clients_.push_back(std::make_unique<ScanningClient>(
        std::move(client), std::move(options), std::move(callback),
        device_scanning_prompt_controller_.get()));
    device_scanning_prompt_controller_->ShowPermissionPrompt();
    return;
  }

  discovery_callback_ = std::move(callback);

  // TODO(https://crbug.com/969109): Since scanning without a filter wastes
  // resources, we need use StartDiscoverySessionWithFilter() instead of
  // StartDiscoverySession() here.
  adapter->StartDiscoverySession(
      base::Bind(&WebBluetoothServiceImpl::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&client),
                 base::Passed(&options)),
      base::Bind(&WebBluetoothServiceImpl::OnDiscoverySessionError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void WebBluetoothServiceImpl::OnStartDiscoverySession(
    mojo::AssociatedRemote<blink::mojom::WebBluetoothScanClient> client,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    std::unique_ptr<device::BluetoothDiscoverySession> session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!discovery_session_);

  discovery_session_ = std::move(session);

  if (AreScanFiltersAllowed(options->filters)) {
    auto scanning_client = std::make_unique<ScanningClient>(
        std::move(client), std::move(options), std::move(discovery_callback_),
        nullptr);
    scanning_client->RunRequestScanningStartCallback(
        blink::mojom::WebBluetoothResult::SUCCESS);
    scanning_client->set_allow_send_event(true);
    scanning_clients_.push_back(std::move(scanning_client));
    return;
  }

  device_scanning_prompt_controller_ =
      std::make_unique<BluetoothDeviceScanningPromptController>(
          this, render_frame_host_);

  scanning_clients_.push_back(std::make_unique<ScanningClient>(
      std::move(client), std::move(options), std::move(discovery_callback_),
      device_scanning_prompt_controller_.get()));
  device_scanning_prompt_controller_->ShowPermissionPrompt();
}

void WebBluetoothServiceImpl::OnDiscoverySessionError() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device_scanning_prompt_controller_.reset();

  auto result = blink::mojom::RequestScanningStartResult::NewErrorResult(
      blink::mojom::WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
  std::move(discovery_callback_).Run(std::move(result));
}

void WebBluetoothServiceImpl::RequestDeviceImpl(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    RequestDeviceCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  // The renderer should never send invalid options.
  if (IsRequestDeviceOptionsInvalid(options)) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  // Calls to requestDevice() require user activation (user gestures).  We
  // should close any opened chooser when a duplicate requestDevice call is made
  // with the same user activation or when any gesture occurs outside of the
  // opened chooser. This does not happen on all platforms so we don't DCHECK
  // that the old one is closed.  We destroy the old chooser before constructing
  // the new one to make sure they can't conflict.
  device_chooser_controller_.reset();

  device_chooser_controller_.reset(new BluetoothDeviceChooserController(
      this, render_frame_host_, std::move(adapter)));

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  device_chooser_controller_->GetDevice(
      std::move(options),
      base::Bind(&WebBluetoothServiceImpl::OnGetDeviceSuccess,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::Bind(&WebBluetoothServiceImpl::OnGetDeviceFailed,
                 weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl(
    const blink::WebBluetoothDeviceId& device_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const base::Optional<BluetoothUUID>& services_uuid,
    RemoteServerGetPrimaryServicesCallback callback,
    device::BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!device->IsGattConnected()) {
    // The device disconnected while discovery was pending. The returned error
    // does not matter because the renderer ignores the error if the device
    // disconnected.
    RecordGetPrimaryServicesOutcome(
        quantity, UMAGetPrimaryServiceOutcome::DEVICE_DISCONNECTED);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND,
                            base::nullopt /* services */);
    return;
  }

  DCHECK(device->IsGattServicesDiscoveryComplete());

  std::vector<device::BluetoothRemoteGattService*> services =
      services_uuid ? device->GetPrimaryServicesByUUID(services_uuid.value())
                    : device->GetPrimaryServices();

  std::vector<blink::mojom::WebBluetoothRemoteGATTServicePtr> response_services;
  for (device::BluetoothRemoteGattService* service : services) {
    if (!allowed_devices().IsAllowedToAccessService(device_id,
                                                    service->GetUUID())) {
      continue;
    }
    std::string service_instance_id = service->GetIdentifier();
    const std::string& device_address = device->GetAddress();
    auto insert_result = service_id_to_device_address_.insert(
        make_pair(service_instance_id, device_address));
    // If value is already in map, DCHECK it's valid.
    if (!insert_result.second)
      DCHECK_EQ(insert_result.first->second, device_address);

    blink::mojom::WebBluetoothRemoteGATTServicePtr service_ptr =
        blink::mojom::WebBluetoothRemoteGATTService::New();
    service_ptr->instance_id = service_instance_id;
    service_ptr->uuid = service->GetUUID();
    response_services.push_back(std::move(service_ptr));

    if (quantity == blink::mojom::WebBluetoothGATTQueryQuantity::SINGLE) {
      break;
    }
  }

  if (!response_services.empty()) {
    DVLOG(1) << "Services found in device.";
    RecordGetPrimaryServicesOutcome(quantity,
                                    UMAGetPrimaryServiceOutcome::SUCCESS);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_services));
    return;
  }

  DVLOG(1) << "Services not found in device.";
  RecordGetPrimaryServicesOutcome(
      quantity, services_uuid ? UMAGetPrimaryServiceOutcome::NOT_FOUND
                              : UMAGetPrimaryServiceOutcome::NO_SERVICES);
  std::move(callback).Run(
      services_uuid ? blink::mojom::WebBluetoothResult::SERVICE_NOT_FOUND
                    : blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND,
      base::nullopt /* services */);
}

void WebBluetoothServiceImpl::OnGetDeviceSuccess(
    RequestDeviceCallback callback,
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const std::string& device_address) {
  device_chooser_controller_.reset();

  const device::BluetoothDevice* const device =
      GetAdapter()->GetDevice(device_address);
  if (device == nullptr) {
    DVLOG(1) << "Device " << device_address << " no longer in adapter";
    RecordRequestDeviceOutcome(UMARequestDeviceOutcome::CHOSEN_DEVICE_VANISHED);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::CHOSEN_DEVICE_VANISHED,
        nullptr /* device */);
    return;
  }

  const blink::WebBluetoothDeviceId device_id =
      allowed_devices().AddDevice(device_address, options);

  DVLOG(1) << "Device: " << device->GetNameForDisplay();

  auto web_bluetooth_device = blink::mojom::WebBluetoothDevice::New();
  web_bluetooth_device->id = device_id;
  web_bluetooth_device->name = device->GetName();

  RecordRequestDeviceOutcome(UMARequestDeviceOutcome::SUCCESS);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                          std::move(web_bluetooth_device));
}

void WebBluetoothServiceImpl::OnGetDeviceFailed(
    RequestDeviceCallback callback,
    blink::mojom::WebBluetoothResult result) {
  // Errors are recorded by the *device_chooser_controller_.
  std::move(callback).Run(result, nullptr /* device */);
  device_chooser_controller_.reset();
}

void WebBluetoothServiceImpl::OnCreateGATTConnectionSuccess(
    const blink::WebBluetoothDeviceId& device_id,
    base::TimeTicks start_time,
    mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client,
    RemoteServerConnectCallback callback,
    std::unique_ptr<device::BluetoothGattConnection> connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordConnectGATTTimeSuccess(base::TimeTicks::Now() - start_time);
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::SUCCESS);

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    DVLOG(1) << "Already connected.";
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
  connected_devices_->Insert(device_id, std::move(connection),
                             std::move(client));
}

void WebBluetoothServiceImpl::OnCreateGATTConnectionFailed(
    base::TimeTicks start_time,
    RemoteServerConnectCallback callback,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordConnectGATTTimeFailed(base::TimeTicks::Now() - start_time);
  std::move(callback).Run(TranslateConnectErrorAndRecord(error_code));
}

void WebBluetoothServiceImpl::OnCharacteristicReadValueSuccess(
    RemoteCharacteristicReadValueCallback callback,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS, value);
}

void WebBluetoothServiceImpl::OnCharacteristicReadValueFailed(
    RemoteCharacteristicReadValueCallback callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(
      TranslateGATTErrorAndRecord(error_code,
                                  UMAGATTOperation::CHARACTERISTIC_READ),
      base::nullopt /* value */);
}

void WebBluetoothServiceImpl::OnCharacteristicWriteValueSuccess(
    RemoteCharacteristicWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnCharacteristicWriteValueFailed(
    RemoteCharacteristicWriteValueCallback callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::CHARACTERISTIC_WRITE));
}

void WebBluetoothServiceImpl::OnStartNotifySessionSuccess(
    mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
        client,
    RemoteCharacteristicStartNotificationsCallback callback,
    std::unique_ptr<device::BluetoothGattNotifySession> notify_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Copy Characteristic Instance ID before passing a unique pointer because
  // compilers may evaluate arguments in any order.
  std::string characteristic_instance_id =
      notify_session->GetCharacteristicIdentifier();

  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
  // Saving the BluetoothGattNotifySession keeps notifications active.
  auto gatt_notify_session_and_client =
      std::make_unique<GATTNotifySessionAndCharacteristicClient>(
          std::move(notify_session), std::move(client));
  characteristic_id_to_notify_session_[characteristic_instance_id] =
      std::move(gatt_notify_session_and_client);
}

void WebBluetoothServiceImpl::OnStartNotifySessionFailed(
    RemoteCharacteristicStartNotificationsCallback callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::START_NOTIFICATIONS));
}

void WebBluetoothServiceImpl::OnStopNotifySessionComplete(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicStopNotificationsCallback callback) {
  characteristic_id_to_notify_session_.erase(characteristic_instance_id);
  std::move(callback).Run();
}

void WebBluetoothServiceImpl::OnDescriptorReadValueSuccess(
    RemoteDescriptorReadValueCallback callback,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordDescriptorReadValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS, value);
}

void WebBluetoothServiceImpl::OnDescriptorReadValueFailed(
    RemoteDescriptorReadValueCallback callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(TranslateGATTErrorAndRecord(
                              error_code, UMAGATTOperation::DESCRIPTOR_READ),
                          base::nullopt /* value */);
}

void WebBluetoothServiceImpl::OnDescriptorWriteValueSuccess(
    RemoteDescriptorWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(667319): We are reporting failures to UMA but not reporting successes
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnDescriptorWriteValueFailed(
    RemoteDescriptorWriteValueCallback callback,
    device::BluetoothRemoteGattService::GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordDescriptorWriteValueOutcome(UMAGATTOperationOutcome::SUCCESS);
  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::DESCRIPTOR_WRITE));
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForDevice(
    const blink::WebBluetoothDeviceId& device_id) {
  const std::string& device_address =
      allowed_devices().GetDeviceAddress(device_id);
  if (device_address.empty()) {
    CrashRendererAndClosePipe(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result;
  result.device = GetAdapter()->GetDevice(device_address);

  // When a device can't be found in the BluetoothAdapter, that generally
  // indicates that it's gone out of range. We reject with a NetworkError in
  // that case.
  if (result.device == nullptr) {
    result.outcome = CacheQueryOutcome::NO_DEVICE;
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForService(
    const std::string& service_instance_id) {
  auto device_iter = service_id_to_device_address_.find(service_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (device_iter == service_id_to_device_address_.end()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_SERVICE_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  const blink::WebBluetoothDeviceId* device_id =
      allowed_devices().GetDeviceId(device_iter->second);
  // Kill the renderer if origin is not allowed to access the device.
  if (device_id == nullptr) {
    CrashRendererAndClosePipe(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForDevice(*device_id);
  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.service = result.device->GetGattService(service_instance_id);
  if (result.service == nullptr) {
    result.outcome = CacheQueryOutcome::NO_SERVICE;
  } else if (!allowed_devices().IsAllowedToAccessService(
                 *device_id, result.service->GetUUID())) {
    CrashRendererAndClosePipe(bad_message::BDH_SERVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForCharacteristic(
    const std::string& characteristic_instance_id) {
  auto characteristic_iter =
      characteristic_id_to_service_id_.find(characteristic_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (characteristic_iter == characteristic_id_to_service_id_.end()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForService(characteristic_iter->second);

  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.characteristic =
      result.service->GetCharacteristic(characteristic_instance_id);

  if (result.characteristic == nullptr) {
    result.outcome = CacheQueryOutcome::NO_CHARACTERISTIC;
  }

  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForDescriptor(
    const std::string& descriptor_instance_id) {
  auto descriptor_iter =
      descriptor_id_to_characteristic_id_.find(descriptor_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (descriptor_iter == descriptor_id_to_characteristic_id_.end()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_DESCRIPTOR_ID);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result =
      QueryCacheForCharacteristic(descriptor_iter->second);

  if (result.outcome != CacheQueryOutcome::SUCCESS) {
    return result;
  }

  result.descriptor =
      result.characteristic->GetDescriptor(descriptor_instance_id);

  if (result.descriptor == nullptr) {
    result.outcome = CacheQueryOutcome::NO_DESCRIPTOR;
  }

  return result;
}

void WebBluetoothServiceImpl::RunPendingPrimaryServicesRequests(
    device::BluetoothDevice* device) {
  const std::string& device_address = device->GetAddress();

  auto iter = pending_primary_services_requests_.find(device_address);
  if (iter == pending_primary_services_requests_.end()) {
    return;
  }
  std::vector<PrimaryServicesRequestCallback> requests =
      std::move(iter->second);
  pending_primary_services_requests_.erase(iter);

  for (PrimaryServicesRequestCallback& request : requests) {
    std::move(request).Run(device);
  }

  // Sending get-service responses unexpectedly queued another request.
  DCHECK(!base::Contains(pending_primary_services_requests_, device_address));
}

RenderProcessHost* WebBluetoothServiceImpl::GetRenderProcessHost() {
  return render_frame_host_->GetProcess();
}

device::BluetoothAdapter* WebBluetoothServiceImpl::GetAdapter() {
  return BluetoothAdapterFactoryWrapper::Get().GetAdapter(this);
}

void WebBluetoothServiceImpl::CrashRendererAndClosePipe(
    bad_message::BadMessageReason reason) {
  bad_message::ReceivedBadMessage(GetRenderProcessHost(), reason);
  receiver_.reset();
}

url::Origin WebBluetoothServiceImpl::GetOrigin() {
  return render_frame_host_->GetLastCommittedOrigin();
}

BluetoothAllowedDevices& WebBluetoothServiceImpl::allowed_devices() {
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      BrowserContext::GetDefaultStoragePartition(
          web_contents()->GetBrowserContext()));
  scoped_refptr<BluetoothAllowedDevicesMap> allowed_devices_map =
      partition->GetBluetoothAllowedDevicesMap();
  return allowed_devices_map->GetOrCreateAllowedDevices(GetOrigin());
}

void WebBluetoothServiceImpl::StoreAllowedScanOptions(
    const blink::mojom::WebBluetoothRequestLEScanOptions& options) {
  if (options.filters.has_value()) {
    for (const auto& filter : options.filters.value())
      allowed_scan_filters_.push_back(filter.Clone());
  } else {
    accept_all_advertisements_ = true;
  }
}

bool WebBluetoothServiceImpl::AreScanFiltersAllowed(
    const base::Optional<ScanFilters>& filters) const {
  if (accept_all_advertisements_) {
    // Previously allowed accepting all advertisements and no filters. In this
    // case since filtered advertisements are a subset of all advertisements,
    // any filters should be allowed.
    return true;
  }

  if (!filters.has_value()) {
    // |acceptAllAdvertisements| is set in the Bluetooth scanning options, but
    // accepting all advertisements has not been allowed yet, in this case the
    // permission prompt needs to be shown to the user.
    return false;
  }

  // If each |filter| in |filters| can be found in |allowed_scan_filters_|,
  // then |filters| are allowed, otherwise |filters| are not allowed.
  for (const auto& filter : filters.value()) {
    bool allowed = false;
    for (const auto& allowed_filter : allowed_scan_filters_) {
      if (AreScanFiltersSame(*filter, *allowed_filter)) {
        allowed = true;
        break;
      }
    }

    if (!allowed)
      return false;
  }

  return true;
}

void WebBluetoothServiceImpl::ClearState() {
  // Releasing the adapter will drop references to callbacks that have not yet
  // been executed. The receiver must be closed first so that this is allowed.
  receiver_.reset();

  characteristic_id_to_notify_session_.clear();
  pending_primary_services_requests_.clear();
  descriptor_id_to_characteristic_id_.clear();
  characteristic_id_to_service_id_.clear();
  service_id_to_device_address_.clear();
  connected_devices_.reset(
      new FrameConnectedBluetoothDevices(render_frame_host_));
  device_chooser_controller_.reset();
  device_scanning_prompt_controller_.reset();
  scanning_clients_.clear();
  allowed_scan_filters_.clear();
  accept_all_advertisements_ = false;
  BluetoothAdapterFactoryWrapper::Get().ReleaseAdapter(this);
}

}  // namespace content
