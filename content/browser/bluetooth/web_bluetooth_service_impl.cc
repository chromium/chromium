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
#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/bluetooth_util.h"
#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"
#include "content/browser/permissions/permission_controller_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

using device::BluetoothGattCharacteristic;
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

bool IsValidFilter(const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  // At least one member needs to be present.
  if (!filter->name && !filter->name_prefix && !filter->services)
    return false;

  // The renderer will never send a |name| or a |name_prefix| longer than
  // kMaxLengthForDeviceName.
  if (filter->name && filter->name->size() > kMaxLengthForDeviceName)
    return false;

  if (filter->name_prefix &&
      filter->name_prefix->size() > kMaxLengthForDeviceName)
    return false;

  // The |name_prefix| should not be empty
  if (filter->name_prefix && filter->name_prefix->empty())
    return false;

  return true;
}

bool IsValidRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  if (options->accept_all_devices)
    return !options->filters.has_value();

  return HasValidFilter(options->filters);
}

bool IsValidRequestScanOptions(
    const blink::mojom::WebBluetoothRequestLEScanOptionsPtr& options) {
  if (options->accept_all_advertisements)
    return !options->filters.has_value();

  return HasValidFilter(options->filters);
}

}  // namespace

class WebBluetoothServiceImpl::AdvertisementClient {
 public:
  virtual void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) = 0;

  bool is_connected() { return client_.is_connected(); }

 protected:
  explicit AdvertisementClient(
      WebBluetoothServiceImpl* service,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info)
      : client_(std::move(client_info)),
        web_contents_(static_cast<WebContentsImpl*>(
            WebContents::FromRenderFrameHost(service->render_frame_host_))),
        service_(service) {
    // Using base::Unretained() is safe here because all instances of this class
    // will be owned by |service|.
    client_.set_disconnect_handler(
        base::BindOnce(&WebBluetoothServiceImpl::RemoveDisconnectedClients,
                       base::Unretained(service)));
    web_contents_->IncrementBluetoothScanningSessionsCount();
  }
  virtual ~AdvertisementClient() = default;

  mojo::AssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient> client_;
  WebContentsImpl* web_contents_;
  WebBluetoothServiceImpl* service_;
};

class WebBluetoothServiceImpl::WatchAdvertisementsClient
    : public WebBluetoothServiceImpl::AdvertisementClient {
 public:
  WatchAdvertisementsClient(
      WebBluetoothServiceImpl* service,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_info,
      blink::WebBluetoothDeviceId device_id)
      : AdvertisementClient(service, std::move(client_info)),
        device_id_(device_id) {
    DCHECK(device_id_.IsValid());
  }

  ~WatchAdvertisementsClient() override {
    web_contents_->DecrementBluetoothScanningSessionsCount();
  }

  // AdvertisementClient implementation:
  void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) override {
    if (event.device->id != device_id_)
      return;

    auto filtered_event = event.Clone();
    base::EraseIf(
        filtered_event->uuids, [this](const device::BluetoothUUID& uuid) {
          return !service_->IsAllowedToAccessService(device_id_, uuid);
        });
    base::EraseIf(
        filtered_event->service_data,
        [this](const std::pair<device::BluetoothUUID, std::vector<uint8_t>>&
                   entry) {
          return !service_->IsAllowedToAccessService(device_id_, entry.first);
        });
    base::EraseIf(
        filtered_event->manufacturer_data,
        [this](const std::pair<uint16_t, std::vector<uint8_t>>& entry) {
          return !service_->IsAllowedToAccessManufacturerData(device_id_,
                                                              entry.first);
        });
    client_->AdvertisingEvent(std::move(filtered_event));
  }

  blink::WebBluetoothDeviceId device_id() const { return device_id_; }

 private:
  blink::WebBluetoothDeviceId device_id_;
};

class WebBluetoothServiceImpl::ScanningClient
    : public WebBluetoothServiceImpl::AdvertisementClient {
 public:
  ScanningClient(WebBluetoothServiceImpl* service,
                 mojo::PendingAssociatedRemote<
                     blink::mojom::WebBluetoothAdvertisementClient> client_info,
                 blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
                 RequestScanningStartCallback callback)
      : AdvertisementClient(service, std::move(client_info)),
        options_(std::move(options)),
        callback_(std::move(callback)) {
    DCHECK(options_->filters.has_value() ||
           options_->accept_all_advertisements);
  }

  ~ScanningClient() override {
    web_contents_->DecrementBluetoothScanningSessionsCount();
  }

  void SetPromptController(
      BluetoothDeviceScanningPromptController* prompt_controller) {
    prompt_controller_ = prompt_controller;
  }

  // AdvertisingClient implementation:
  void SendEvent(
      const blink::mojom::WebBluetoothAdvertisingEvent& event) override {
    // TODO(https://crbug.com/1108958): Filter out advertisement data if not
    // included in the filters, optionalServices, or optionalManufacturerData.
    auto filtered_event = event.Clone();
    if (options_->accept_all_advertisements) {
      if (prompt_controller_)
        AddFilteredDeviceToPrompt(filtered_event->device->id.str(),
                                  filtered_event->name);

      if (allow_send_event_)
        client_->AdvertisingEvent(std::move(filtered_event));

      return;
    }

    DCHECK(options_->filters.has_value());

    // For every filter, we're going to check to see if a |name|, |name_prefix|,
    // or |services| have been set. If one of these is set, we will check the
    // scan result to see if it matches the filter's value.  If it doesn't,
    // we'll just continue with the next filter. If all of the properties in a
    // filter have a match, we can post the AdvertisingEvent. Otherwise, we are
    // going to drop it. This logic can be reduced a bit, but I think clarity
    // will decrease.
    for (auto& filter : options_->filters.value()) {
      // Check to see if there is a direct match against the advertisement name
      if (filter->name.has_value()) {
        if (!filtered_event->name.has_value() ||
            filter->name.value() != filtered_event->name.value()) {
          continue;
        }
      }

      // Check if there is a name prefix match
      if (filter->name_prefix.has_value()) {
        if (!filtered_event->name.has_value() ||
            !base::StartsWith(filtered_event->name.value(),
                              filter->name_prefix.value(),
                              base::CompareCase::SENSITIVE)) {
          continue;
        }
      }

      // Check to see if there is a service uuid match
      if (filter->services.has_value()) {
        auto it = std::find_if(
            filter->services.value().begin(), filter->services.value().end(),
            [&filtered_event](const BluetoothUUID& filter_uuid) {
              return base::Contains(filtered_event->uuids, filter_uuid);
            });
        if (it == filter->services.value().end())
          continue;
      }

      // TODO(crbug.com/707635): Support manufacturerData and serviceData
      // filters.

      if (prompt_controller_)
        AddFilteredDeviceToPrompt(filtered_event->device->id.str(),
                                  filtered_event->name);

      if (allow_send_event_)
        client_->AdvertisingEvent(std::move(filtered_event));
      return;
    }
  }

  void RunRequestScanningStartCallback(
      blink::mojom::WebBluetoothResult result) {
    DCHECK(result == blink::mojom::WebBluetoothResult::SUCCESS ||
           result == blink::mojom::WebBluetoothResult::SCANNING_BLOCKED ||
           result == blink::mojom::WebBluetoothResult::PROMPT_CANCELED);
    std::move(callback_).Run(result);
  }

  void set_prompt_controller(
      BluetoothDeviceScanningPromptController* prompt_controller) {
    prompt_controller_ = prompt_controller;
  }

  BluetoothDeviceScanningPromptController* prompt_controller() {
    return prompt_controller_;
  }

  void set_allow_send_event(bool allow_send_event) {
    allow_send_event_ = allow_send_event;
  }

  const blink::mojom::WebBluetoothRequestLEScanOptions& scan_options() {
    return *options_;
  }

 private:
  void AddFilteredDeviceToPrompt(
      const std::string& device_id,
      const base::Optional<std::string>& device_name) {
    bool should_update_name = device_name.has_value();
    base::string16 device_name_for_display =
        base::UTF8ToUTF16(device_name.value_or(""));
    prompt_controller_->AddFilteredDevice(device_id, should_update_name,
                                          device_name_for_display);
  }

  bool allow_send_event_ = false;
  blink::mojom::WebBluetoothRequestLEScanOptionsPtr options_;
  RequestScanningStartCallback callback_;
  BluetoothDeviceScanningPromptController* prompt_controller_ = nullptr;
};

bool HasValidFilter(
    const base::Optional<
        std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters) {
  if (!filters) {
    return false;
  }

  return !filters->empty() &&
         std::all_of(filters->begin(), filters->end(), IsValidFilter);
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
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return false;
    return delegate->GetWebBluetoothDeviceId(render_frame_host_, device_address)
        .IsValid();
  }
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
    ClearAdvertisementClients();
  }
}

void WebBluetoothServiceImpl::OnWebContentsLostFocus(
    RenderWidgetHost* render_widget_host) {
  ClearAdvertisementClients();
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

  if (!HasActiveDiscoverySession())
    return;

  // Construct the WebBluetoothAdvertisingEvent.
  auto device = blink::mojom::WebBluetoothDevice::New();
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return;
    device->id = delegate->AddScannedDevice(render_frame_host_, device_address);
  } else {
    device->id = allowed_devices().AddDevice(device_address);
  }
  device->name = device_name;

  auto result = blink::mojom::WebBluetoothAdvertisingEvent::New();
  result->device = std::move(device);

  result->name = advertisement_name;

  // Note about the default value for these optional types. On the other side of
  // this IPC, the receiver will be checking to see if |*_is_set| is true before
  // using the value. Here we chose reasonable defaults in case the other side
  // does something incorrect. We have to do this manual serialization because
  // mojo does not support optional primitive types.
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
  manufacturer_data.insert(manufacturer_data_map.begin(),
                           manufacturer_data_map.end());

  auto& service_data = result->service_data;
  service_data.insert(service_data_map.begin(), service_data_map.end());

  // TODO(https://crbug.com/1087007): These two classes can potentially be
  // combined into the same container.
  for (const auto& scanning_client : scanning_clients_)
    scanning_client->SendEvent(*result);

  for (const auto& watch_advertisements_client : watch_advertisements_clients_)
    watch_advertisements_client->SendEvent(*result);

  MaybeStopDiscovery();
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

void WebBluetoothServiceImpl::GetDevices(GetDevicesCallback callback) {
  if (GetBluetoothAllowed() != blink::mojom::WebBluetoothResult::SUCCESS ||
      !BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
    std::move(callback).Run({});
    return;
  }

  auto* adapter = GetAdapter();
  if (adapter) {
    GetDevicesImpl(std::move(callback), adapter);
    return;
  }

  BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
      this,
      base::BindOnce(&WebBluetoothServiceImpl::GetDevicesImpl,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebBluetoothServiceImpl::RemoteServerConnect(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothServerClient>
        client,
    RemoteServerConnectCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  bool is_connect_allowed = false;
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (delegate) {
      is_connect_allowed =
          delegate->HasDevicePermission(render_frame_host_, device_id);
    }
  } else {
    is_connect_allowed = allowed_devices().IsAllowedToGATTConnect(device_id);
  }
  if (!is_connect_allowed) {
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
  // the callee interface. The |callback| will only be called once, but it is
  // passed to both the success and error callbacks.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  query_result.device->CreateGattConnection(
      base::BindOnce(&WebBluetoothServiceImpl::OnCreateGATTConnectionSuccess,
                     weak_ptr_factory_.GetWeakPtr(), device_id, start_time,
                     std::move(web_bluetooth_server_client), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnCreateGATTConnectionFailed,
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

  if (!IsAllowedToAccessAtLeastOneService(device_id)) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE,
        /*service=*/base::nullopt);
    return;
  }

  if (services_uuid &&
      !IsAllowedToAccessService(device_id, services_uuid.value())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_SERVICE,
        /*service=*/base::nullopt);
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
      base::BindOnce(&WebBluetoothServiceImpl::OnCharacteristicReadValueSuccess,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnCharacteristicReadValueFailed,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicWriteValue(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    blink::mojom::WebBluetoothWriteType write_type,
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
  base::OnceClosure write_callback = base::BindOnce(
      &WebBluetoothServiceImpl::OnCharacteristicWriteValueSuccess,
      weak_ptr_factory_.GetWeakPtr(), copyable_callback);
  device::BluetoothGattCharacteristic::ErrorCallback write_error_callback =
      base::BindOnce(&WebBluetoothServiceImpl::OnCharacteristicWriteValueFailed,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback);
  using WebBluetoothWriteType = blink::mojom::WebBluetoothWriteType;
  using WriteType = device::BluetoothRemoteGattCharacteristic::WriteType;
  switch (write_type) {
    case WebBluetoothWriteType::kWriteDefaultDeprecated:
      query_result.characteristic->DeprecatedWriteRemoteCharacteristic(
          value, std::move(write_callback), std::move(write_error_callback));
      break;
    case WebBluetoothWriteType::kWriteWithResponse:
      query_result.characteristic->WriteRemoteCharacteristic(
          value, WriteType::kWithResponse, std::move(write_callback),
          std::move(write_error_callback));
      break;
    case WebBluetoothWriteType::kWriteWithoutResponse:
      query_result.characteristic->WriteRemoteCharacteristic(
          value, WriteType::kWithoutResponse, std::move(write_callback),
          std::move(write_error_callback));
      break;
  }
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
      base::BindOnce(&WebBluetoothServiceImpl::OnStartNotifySessionSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(characteristic_client), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnStartNotifySessionFailed,
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
      base::BindOnce(&WebBluetoothServiceImpl::OnStopNotifySessionComplete,
                     weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
                     std::move(callback)));
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
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorReadValueSuccess,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorReadValueFailed,
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
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorWriteValueSuccess,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorWriteValueFailed,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::RequestScanningStart(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_info,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const url::Origin requesting_origin =
      render_frame_host_->GetLastCommittedOrigin();
  const url::Origin embedding_origin =
      web_contents()->GetMainFrame()->GetLastCommittedOrigin();

  bool blocked = GetContentClient()->browser()->IsBluetoothScanningBlocked(
      web_contents()->GetBrowserContext(), requesting_origin, embedding_origin);
  if (blocked) {
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SCANNING_BLOCKED);
    return;
  }

  // The renderer should never send invalid options.
  if (!IsValidRequestScanOptions(options)) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this,
          base::BindOnce(&WebBluetoothServiceImpl::RequestScanningStartImpl,
                         weak_ptr_factory_.GetWeakPtr(), std::move(client_info),
                         std::move(options), std::move(callback)));
      return;
    }
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  RequestScanningStartImpl(std::move(client_info), std::move(options),
                           std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::WatchAdvertisementsForDevice(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_info,
    WatchAdvertisementsForDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::mojom::WebBluetoothResult allowed_result = GetBluetoothAllowed();
  if (allowed_result != blink::mojom::WebBluetoothResult::SUCCESS) {
    std::move(callback).Run(allowed_result);
    return;
  }

  // The renderer should never send an invalid |device_id|.
  if (!device_id.IsValid()) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this, base::BindOnce(
                    &WebBluetoothServiceImpl::WatchAdvertisementsForDeviceImpl,
                    weak_ptr_factory_.GetWeakPtr(), device_id,
                    std::move(client_info), std::move(callback)));
      return;
    }
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  WatchAdvertisementsForDeviceImpl(std::move(device_id), std::move(client_info),
                                   std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::RemoveDisconnectedClients() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(https://crbug.com/1087007): These two classes can potentially be
  // combined into the same container.
  base::EraseIf(scanning_clients_,
                [](const std::unique_ptr<ScanningClient>& client) {
                  return !client->is_connected();
                });
  base::EraseIf(watch_advertisements_clients_,
                [](const std::unique_ptr<WatchAdvertisementsClient>& client) {
                  return !client->is_connected();
                });
  MaybeStopDiscovery();
}

void WebBluetoothServiceImpl::MaybeStopDiscovery() {
  if (scanning_clients_.empty())
    ble_scan_discovery_session_.reset();

  if (watch_advertisements_clients_.empty())
    watch_advertisements_discovery_session_.reset();
}

void WebBluetoothServiceImpl::RequestScanningStartImpl(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_info,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!adapter) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  if (request_scanning_start_callback_) {
    std::move(callback).Run(blink::mojom::WebBluetoothResult::PROMPT_CANCELED);
    return;
  }

  if (ble_scan_discovery_session_) {
    auto scanning_client = std::make_unique<ScanningClient>(
        /*service=*/this, std::move(client_info), std::move(options),
        std::move(callback));

    if (AreScanFiltersAllowed(scanning_client->scan_options().filters)) {
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
    scanning_client->SetPromptController(
        device_scanning_prompt_controller_.get());
    scanning_clients_.push_back(std::move(scanning_client));
    device_scanning_prompt_controller_->ShowPermissionPrompt();
    return;
  }

  request_scanning_start_callback_ = std::move(callback);

  // TODO(https://crbug.com/969109): Since scanning without a filter wastes
  // resources, we need use StartDiscoverySessionWithFilter() instead of
  // StartDiscoverySession() here.
  adapter->StartDiscoverySession(
      base::BindOnce(
          &WebBluetoothServiceImpl::OnStartDiscoverySessionForScanning,
          weak_ptr_factory_.GetWeakPtr(), std::move(client_info),
          std::move(options)),
      base::BindOnce(
          &WebBluetoothServiceImpl::OnDiscoverySessionErrorForScanning,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebBluetoothServiceImpl::OnStartDiscoverySessionForScanning(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_info,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    std::unique_ptr<device::BluetoothDiscoverySession> session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!ble_scan_discovery_session_);

  ble_scan_discovery_session_ = std::move(session);

  auto scanning_client = std::make_unique<ScanningClient>(
      /*service=*/this, std::move(client_info), std::move(options),
      std::move(request_scanning_start_callback_));

  if (AreScanFiltersAllowed(scanning_client->scan_options().filters)) {
    scanning_client->RunRequestScanningStartCallback(
        blink::mojom::WebBluetoothResult::SUCCESS);
    scanning_client->set_allow_send_event(true);
    scanning_clients_.push_back(std::move(scanning_client));
    return;
  }

  device_scanning_prompt_controller_ =
      std::make_unique<BluetoothDeviceScanningPromptController>(
          this, render_frame_host_);
  scanning_client->SetPromptController(
      device_scanning_prompt_controller_.get());
  scanning_clients_.push_back(std::move(scanning_client));
  device_scanning_prompt_controller_->ShowPermissionPrompt();
}

void WebBluetoothServiceImpl::OnDiscoverySessionErrorForScanning() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  device_scanning_prompt_controller_.reset();

  std::move(request_scanning_start_callback_)
      .Run(blink::mojom::WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
  ClearAdvertisementClients();
}

void WebBluetoothServiceImpl::RequestDeviceImpl(
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    RequestDeviceCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  // The renderer should never send invalid options.
  if (!IsValidRequestDeviceOptions(options)) {
    CrashRendererAndClosePipe(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  // Calls to requestDevice() require user activation (user gestures).  We
  // should close any opened chooser when a duplicate requestDevice call is
  // made with the same user activation or when any gesture occurs outside
  // of the opened chooser. This does not happen on all platforms so we
  // don't DCHECK that the old one is closed.  We destroy the old chooser
  // before constructing the new one to make sure they can't conflict.
  device_chooser_controller_.reset();

  device_chooser_controller_.reset(new BluetoothDeviceChooserController(
      this, render_frame_host_, std::move(adapter)));

  // TODO(crbug.com/730593): Remove AdaptCallbackForRepeating() by updating
  // the callee interface.
  auto copyable_callback = base::AdaptCallbackForRepeating(std::move(callback));
  device_chooser_controller_->GetDevice(
      std::move(options),
      base::BindOnce(&WebBluetoothServiceImpl::OnGetDeviceSuccess,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback),
      base::BindOnce(&WebBluetoothServiceImpl::OnGetDeviceFailed,
                     weak_ptr_factory_.GetWeakPtr(), copyable_callback));
}

void WebBluetoothServiceImpl::GetDevicesImpl(
    GetDevicesCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate) {
      std::move(callback).Run({});
      return;
    }

    std::move(callback).Run(delegate->GetPermittedDevices(render_frame_host_));
    return;
  }

  // BluetoothAllowedDevices does not provide a way to get all of the
  // permitted devices, so instead return all of the allowed devices that
  // are currently known to the system.
  std::vector<blink::mojom::WebBluetoothDevicePtr> web_bluetooth_devices;
  for (const auto* device : adapter->GetDevices()) {
    const blink::WebBluetoothDeviceId* device_id =
        allowed_devices().GetDeviceId(device->GetAddress());
    if (!device_id || !allowed_devices().IsAllowedToGATTConnect(*device_id))
      continue;

    web_bluetooth_devices.push_back(
        blink::mojom::WebBluetoothDevice::New(*device_id, device->GetName()));
  }
  std::move(callback).Run(std::move(web_bluetooth_devices));
}

void WebBluetoothServiceImpl::WatchAdvertisementsForDeviceImpl(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_info,
    WatchAdvertisementsForDeviceCallback callback,
    scoped_refptr<device::BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!adapter) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  auto watch_advertisements_client =
      std::make_unique<WatchAdvertisementsClient>(
          /*service=*/this, std::move(client_info), std::move(device_id));
  if (watch_advertisements_discovery_session_) {
    watch_advertisements_clients_.push_back(
        std::move(watch_advertisements_client));
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  // If |watch_advertismeents_callbacks_and_clients_| has more than one entry,
  // then it means that a previous watch advertisements operation has already
  // started a discovery session, so the |callback| and |client| for this
  // operation needs to be stored until the start discovery operation is
  // complete.
  watch_advertisements_callbacks_and_clients_.emplace_back(
      std::move(callback), std::move(watch_advertisements_client));
  if (watch_advertisements_callbacks_and_clients_.size() > 1)
    return;

  // Not all platforms support filtering by address.
  // TODO(https://crbug.com/969109): Use StartDiscoverySessionWithFilter() to
  // filter out by MAC address when platforms provide this capability.
  adapter->StartDiscoverySession(
      base::BindOnce(&WebBluetoothServiceImpl::
                         OnStartDiscoverySessionForWatchAdvertisements,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebBluetoothServiceImpl::
                         OnDiscoverySessionErrorForWatchAdvertisements,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebBluetoothServiceImpl::OnStartDiscoverySessionForWatchAdvertisements(
    std::unique_ptr<device::BluetoothDiscoverySession> session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!watch_advertisements_discovery_session_);
  watch_advertisements_discovery_session_ = std::move(session);

  for (auto& callback_and_client :
       watch_advertisements_callbacks_and_clients_) {
    if (callback_and_client.second->is_connected()) {
      watch_advertisements_clients_.push_back(
          std::move(callback_and_client.second));
      std::move(callback_and_client.first)
          .Run(blink::mojom::WebBluetoothResult::SUCCESS);
      continue;
    }

    std::move(callback_and_client.first)
        .Run(blink::mojom::WebBluetoothResult::WATCH_ADVERTISEMENTS_ABORTED);
  }

  watch_advertisements_callbacks_and_clients_.clear();

  // If a client was disconncted while a discovery session was being started,
  // then there may not be any valid clients, so discovery should be stopped.
  MaybeStopDiscovery();
}

void WebBluetoothServiceImpl::OnDiscoverySessionErrorForWatchAdvertisements() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& callback_and_client :
       watch_advertisements_callbacks_and_clients_) {
    std::move(callback_and_client.first)
        .Run(blink::mojom::WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
  }
  watch_advertisements_callbacks_and_clients_.clear();
  ClearAdvertisementClients();
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
    if (!IsAllowedToAccessService(device_id, service->GetUUID()))
      continue;

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

  DVLOG(1) << "Device: " << device->GetNameForDisplay();

  auto web_bluetooth_device = blink::mojom::WebBluetoothDevice::New();
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate) {
      std::move(callback).Run(
          blink::mojom::WebBluetoothResult::WEB_BLUETOOTH_NOT_SUPPORTED,
          /*device=*/nullptr);
      return;
    }
    web_bluetooth_device->id = delegate->GrantServiceAccessPermission(
        render_frame_host_, device, options.get());
  } else {
    web_bluetooth_device->id =
        allowed_devices().AddDevice(device_address, options);
  }
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
  std::string device_address;
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (delegate) {
      device_address =
          delegate->GetDeviceAddress(render_frame_host_, device_id);
    }
  } else {
    device_address = allowed_devices().GetDeviceAddress(device_id);
  }

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

  blink::WebBluetoothDeviceId device_id;
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (delegate) {
      device_id = delegate->GetWebBluetoothDeviceId(render_frame_host_,
                                                    device_iter->second);
    }
  } else {
    const blink::WebBluetoothDeviceId* device_id_ptr =
        allowed_devices().GetDeviceId(device_iter->second);
    if (device_id_ptr)
      device_id = *device_id_ptr;
  }
  // Kill the renderer if origin is not allowed to access the device.
  if (!device_id.IsValid()) {
    CrashRendererAndClosePipe(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::BAD_RENDERER);
  }

  CacheQueryResult result = QueryCacheForDevice(device_id);
  if (result.outcome != CacheQueryOutcome::SUCCESS)
    return result;

  result.service = result.device->GetGattService(service_instance_id);
  if (result.service == nullptr) {
    result.outcome = CacheQueryOutcome::NO_SERVICE;
    return result;
  }

  if (!IsAllowedToAccessService(device_id, result.service->GetUUID())) {
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
  return partition->GetBluetoothAllowedDevicesMap()->GetOrCreateAllowedDevices(
      GetOrigin());
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

  // If each |filter| in |filters| can be found in |allowed_scan_filters_|, then
  // |filters| are allowed, otherwise |filters| are not allowed.
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
  ClearAdvertisementClients();
  BluetoothAdapterFactoryWrapper::Get().ReleaseAdapter(this);
}

void WebBluetoothServiceImpl::ClearAdvertisementClients() {
  scanning_clients_.clear();
  watch_advertisements_clients_.clear();
  allowed_scan_filters_.clear();
  accept_all_advertisements_ = false;
}

bool WebBluetoothServiceImpl::IsAllowedToAccessAtLeastOneService(
    const blink::WebBluetoothDeviceId& device_id) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return false;
    return delegate->IsAllowedToAccessAtLeastOneService(render_frame_host_,
                                                        device_id);
  }
  return allowed_devices().IsAllowedToAccessAtLeastOneService(device_id);
}

bool WebBluetoothServiceImpl::IsAllowedToAccessService(
    const blink::WebBluetoothDeviceId& device_id,
    const device::BluetoothUUID& service) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return false;
    return delegate->IsAllowedToAccessService(render_frame_host_, device_id,
                                              service);
  }
  return allowed_devices().IsAllowedToAccessService(device_id, service);
}

bool WebBluetoothServiceImpl::IsAllowedToAccessManufacturerData(
    const blink::WebBluetoothDeviceId& device_id,
    uint16_t manufacturer_code) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return false;
    return delegate->IsAllowedToAccessManufacturerData(
        render_frame_host_, device_id, manufacturer_code);
  }
  return allowed_devices().IsAllowedToAccessManufacturerData(device_id,
                                                             manufacturer_code);
}

bool WebBluetoothServiceImpl::HasActiveDiscoverySession() {
  return (ble_scan_discovery_session_ &&
          ble_scan_discovery_session_->IsActive()) ||
         (watch_advertisements_discovery_session_ &&
          watch_advertisements_discovery_session_->IsActive());
}

}  // namespace content
