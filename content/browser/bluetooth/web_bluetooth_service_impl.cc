// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ID Not In Map Note: A service, characteristic, or descriptor ID not in the
// corresponding WebBluetoothServiceImpl map [service_id_to_device_address_,
// characteristic_id_to_service_id_, descriptor_id_to_characteristic_id_]
// implies a hostile renderer because a renderer obtains the corresponding ID
// from this class and it will be added to the map at that time.

#include "content/browser/bluetooth/web_bluetooth_service_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/queue.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "content/browser/bluetooth/advertisement_client.h"
#include "content/browser/bluetooth/bluetooth_adapter_factory_wrapper.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "content/browser/bluetooth/bluetooth_blocklist.h"
#include "content/browser/bluetooth/bluetooth_device_chooser_controller.h"
#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"
#include "content/browser/bluetooth/bluetooth_metrics.h"
#include "content/browser/bluetooth/bluetooth_util.h"
#include "content/browser/bluetooth/frame_connected_bluetooth_devices.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_impl.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_gatt_connection.h"
#include "device/bluetooth/bluetooth_gatt_notify_session.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"
#include "device/bluetooth/bluetooth_remote_gatt_descriptor.h"
#include "device/bluetooth/bluetooth_remote_gatt_service.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/common/bluetooth/web_bluetooth_device_id.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom.h"

namespace content {

namespace {

using ::device::BluetoothAdapter;
using ::device::BluetoothDevice;
using ::device::BluetoothDiscoverySession;
using ::device::BluetoothGattCharacteristic;
using ::device::BluetoothGattConnection;
using ::device::BluetoothGattNotifySession;
using ::device::BluetoothGattService;
using ::device::BluetoothRemoteGattCharacteristic;
using ::device::BluetoothRemoteGattDescriptor;
using ::device::BluetoothRemoteGattService;
using ::device::BluetoothUUID;
using GattErrorCode = ::device::BluetoothGattService::GattErrorCode;

// Client names for logging in BLE scanning.
constexpr char kScanClientNameWatchAdvertisements[] =
    "Web Bluetooth watchAdvertisements()";
constexpr char kScanClientNameRequestLeScan[] = "Web Bluetooth requestLeScan()";

// The renderer performs its own checks so a request that gets to the browser
// process indicates some failure to check for fenced frames.
const char kFencedFrameError[] =
    "Use of Web Bluetooth API is blocked in a <fencedframe> tree.";

blink::mojom::WebBluetoothResult TranslateGATTErrorAndRecord(
    GattErrorCode error_code,
    UMAGATTOperation operation) {
  switch (error_code) {
    case device::BluetoothRemoteGattService::GattErrorCode::kUnknown:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::kUnknown);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_ERROR;
    case device::BluetoothRemoteGattService::GattErrorCode::kFailed:
      RecordGATTOperationOutcome(operation, UMAGATTOperationOutcome::kFailed);
      return blink::mojom::WebBluetoothResult::GATT_UNKNOWN_FAILURE;
    case device::BluetoothRemoteGattService::GattErrorCode::kInProgress:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kInProgress);
      return blink::mojom::WebBluetoothResult::GATT_OPERATION_IN_PROGRESS;
    case device::BluetoothRemoteGattService::GattErrorCode::kInvalidLength:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kInvalidLength);
      return blink::mojom::WebBluetoothResult::GATT_INVALID_ATTRIBUTE_LENGTH;
    case device::BluetoothRemoteGattService::GattErrorCode::kNotPermitted:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kNotPermitted);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PERMITTED;
    case device::BluetoothRemoteGattService::GattErrorCode::kNotAuthorized:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kNotAuthorized);
      return blink::mojom::WebBluetoothResult::GATT_NOT_AUTHORIZED;
    case device::BluetoothRemoteGattService::GattErrorCode::kNotPaired:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kNotPaired);
      return blink::mojom::WebBluetoothResult::GATT_NOT_PAIRED;
    case device::BluetoothRemoteGattService::GattErrorCode::kNotSupported:
      RecordGATTOperationOutcome(operation,
                                 UMAGATTOperationOutcome::kNotSupported);
      return blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::WebBluetoothResult::GATT_UNTRANSLATED_ERROR_CODE;
}

// Max length of device name in filter. Bluetooth 5.0 3.C.3.2.2.3 states that
// the maximum device name length is 248 bytes (UTF-8 encoded).
constexpr size_t kMaxLengthForDeviceName = 248;

bool IsValidFilter(const blink::mojom::WebBluetoothLeScanFilterPtr& filter) {
  // At least one member needs to be present.
  if (!filter->name && !filter->name_prefix && !filter->services &&
      !filter->manufacturer_data) {
    return false;
  }

  // The |services| should not be empty.
  if (filter->services && filter->services->empty())
    return false;

  // The renderer will never send a |name| or a |name_prefix| longer than
  // kMaxLengthForDeviceName.
  if (filter->name && filter->name->size() > kMaxLengthForDeviceName)
    return false;

  if (filter->name_prefix &&
      filter->name_prefix->size() > kMaxLengthForDeviceName)
    return false;

  // The |name_prefix| should not be empty.
  if (filter->name_prefix && filter->name_prefix->empty())
    return false;

  // The |manufacturer_data| should not be empty.
  if (filter->manufacturer_data && filter->manufacturer_data->empty())
    return false;

  return true;
}

bool IsValidRequestDeviceOptions(
    const blink::mojom::WebBluetoothRequestDeviceOptionsPtr& options) {
  if (options->accept_all_devices)
    return !options->filters.has_value();

  if (!HasValidFilter(options->filters)) {
    return false;
  }

  if (options->exclusion_filters.has_value()) {
    return HasValidFilter(options->exclusion_filters);
  }

  return true;
}

bool IsValidRequestScanOptions(
    const blink::mojom::WebBluetoothRequestLEScanOptionsPtr& options) {
  if (options->accept_all_advertisements)
    return !options->filters.has_value();

  return HasValidFilter(options->filters);
}

bool& ShouldIgnoreVisibilityRequirementsForTesting() {
  static bool should_ignore_visibility_requirements = false;
  return should_ignore_visibility_requirements;
}

}  // namespace

// Parameters for a call to RemoteCharacteristicStartNotifications. Used to
// defer notification starts when one is currently running for the same
// characteristic instance.
struct WebBluetoothServiceImpl::DeferredStartNotificationData {
  DeferredStartNotificationData(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothCharacteristicClient> client,
      RemoteCharacteristicStartNotificationsCallback callback)
      : client(std::move(client)), callback(std::move(callback)) {}

  ~DeferredStartNotificationData() = default;

  DeferredStartNotificationData& operator=(
      const DeferredStartNotificationData&) = delete;

  mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
      client;
  RemoteCharacteristicStartNotificationsCallback callback;
};

// static
blink::mojom::WebBluetoothResult
WebBluetoothServiceImpl::TranslateConnectErrorAndRecord(
    BluetoothDevice::ConnectErrorCode error_code) {
  switch (error_code) {
    case BluetoothDevice::ERROR_UNKNOWN:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kUnknown);
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_ERROR;
    case BluetoothDevice::ERROR_INPROGRESS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kInProgress);
      return blink::mojom::WebBluetoothResult::CONNECT_ALREADY_IN_PROGRESS;
    case BluetoothDevice::ERROR_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kFailed);
      return blink::mojom::WebBluetoothResult::CONNECT_CONN_FAILED;
    case BluetoothDevice::ERROR_AUTH_FAILED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAuthFailed);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_FAILED;
    case BluetoothDevice::ERROR_AUTH_CANCELED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAuthCanceled);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_CANCELED;
    case BluetoothDevice::ERROR_AUTH_REJECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAuthRejected);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_REJECTED;
    case BluetoothDevice::ERROR_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAuthTimeout);
      return blink::mojom::WebBluetoothResult::CONNECT_AUTH_TIMEOUT;
    case BluetoothDevice::ERROR_UNSUPPORTED_DEVICE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kUnsupportedDevice);
      return blink::mojom::WebBluetoothResult::CONNECT_UNSUPPORTED_DEVICE;
    case BluetoothDevice::ERROR_DEVICE_NOT_READY:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kNotReady);
      return blink::mojom::WebBluetoothResult::CONNECT_NOT_READY;
    case BluetoothDevice::ERROR_ALREADY_CONNECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAlreadyConnected);
      return blink::mojom::WebBluetoothResult::CONNECT_ALREADY_CONNECTED;
    case BluetoothDevice::ERROR_DEVICE_ALREADY_EXISTS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kAlreadyExists);
      return blink::mojom::WebBluetoothResult::CONNECT_ALREADY_EXISTS;
    case BluetoothDevice::ERROR_DEVICE_UNCONNECTED:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kNotConnected);
      return blink::mojom::WebBluetoothResult::CONNECT_NOT_CONNECTED;
    case BluetoothDevice::ERROR_DOES_NOT_EXIST:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kDoesNotExist);
      return blink::mojom::WebBluetoothResult::CONNECT_DOES_NOT_EXIST;
    case BluetoothDevice::ERROR_INVALID_ARGS:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kInvalidArgs);
      return blink::mojom::WebBluetoothResult::CONNECT_INVALID_ARGS;
    case BluetoothDevice::ERROR_NON_AUTH_TIMEOUT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kNonAuthTimeout);
      return blink::mojom::WebBluetoothResult::CONNECT_NON_AUTH_TIMEOUT;
    case device::BluetoothDevice::ERROR_NO_MEMORY:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kNoMemory);
      return blink::mojom::WebBluetoothResult::CONNECT_NO_MEMORY;
    case device::BluetoothDevice::ERROR_JNI_ENVIRONMENT:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kJniEnvironment);
      return blink::mojom::WebBluetoothResult::CONNECT_JNI_ENVIRONMENT;
    case device::BluetoothDevice::ERROR_JNI_THREAD_ATTACH:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kJniThreadAttach);
      return blink::mojom::WebBluetoothResult::CONNECT_JNI_THREAD_ATTACH;
    case device::BluetoothDevice::ERROR_WAKELOCK:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kWakelock);
      return blink::mojom::WebBluetoothResult::CONNECT_WAKELOCK;
    case device::BluetoothDevice::ERROR_UNEXPECTED_STATE:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kUnexpectedState);
      return blink::mojom::WebBluetoothResult::CONNECT_UNEXPECTED_STATE;
    case device::BluetoothDevice::ERROR_SOCKET:
      RecordConnectGATTOutcome(UMAConnectGATTOutcome::kSocketError);
      return blink::mojom::WebBluetoothResult::CONNECT_SOCKET_ERROR;
    case BluetoothDevice::NUM_CONNECT_ERROR_CODES:
      NOTREACHED_IN_MIGRATION();
      return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
  }
  NOTREACHED_IN_MIGRATION();
  return blink::mojom::WebBluetoothResult::CONNECT_UNKNOWN_FAILURE;
}

// static
void WebBluetoothServiceImpl::IgnoreVisibilityRequirementsForTesting() {
  ShouldIgnoreVisibilityRequirementsForTesting() = true;
}

bool HasValidFilter(
    const std::optional<std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>&
        filters) {
  if (!filters) {
    return false;
  }

  return !filters->empty() && base::ranges::all_of(*filters, IsValidFilter);
}

// Struct that holds the result of a cache query.
struct CacheQueryResult {
  CacheQueryResult() : outcome(CacheQueryOutcome::kSuccess) {}

  explicit CacheQueryResult(CacheQueryOutcome outcome) : outcome(outcome) {}

  ~CacheQueryResult() {}

  blink::mojom::WebBluetoothResult GetWebResult() const {
    switch (outcome) {
      case CacheQueryOutcome::kSuccess:
      case CacheQueryOutcome::kBadRenderer:
        NOTREACHED_IN_MIGRATION();
        return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::kNoDevice:
        return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
      case CacheQueryOutcome::kNoService:
        return blink::mojom::WebBluetoothResult::SERVICE_NO_LONGER_EXISTS;
      case CacheQueryOutcome::kNoCharacteristic:
        return blink::mojom::WebBluetoothResult::
            CHARACTERISTIC_NO_LONGER_EXISTS;
      case CacheQueryOutcome::kNoDescriptor:
        return blink::mojom::WebBluetoothResult::DESCRIPTOR_NO_LONGER_EXISTS;
    }
    NOTREACHED_IN_MIGRATION();
    return blink::mojom::WebBluetoothResult::DEVICE_NO_LONGER_IN_RANGE;
  }

  raw_ptr<BluetoothDevice> device = nullptr;
  raw_ptr<BluetoothRemoteGattService> service = nullptr;
  raw_ptr<BluetoothRemoteGattCharacteristic> characteristic = nullptr;
  raw_ptr<BluetoothRemoteGattDescriptor> descriptor = nullptr;
  CacheQueryOutcome outcome;
};

struct GATTNotifySessionAndCharacteristicClient {
  GATTNotifySessionAndCharacteristicClient(
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client)
      : characteristic_client(std::move(client)) {}

  std::unique_ptr<BluetoothGattNotifySession> gatt_notify_session;
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
      characteristic_client;
};

// static
void WebBluetoothServiceImpl::BindIfAllowed(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(render_frame_host);

  if (render_frame_host->IsNestedWithinFencedFrame()) {
    // The renderer is supposed to disallow the use of web bluetooth when inside
    // a fenced frame. Anything getting past the renderer checks must be marked
    // as a bad request.
    mojo::ReportBadMessage(kFencedFrameError);
    return;
  }

  if (render_frame_host->GetOutermostMainFrame()
          ->GetLastCommittedOrigin()
          .opaque()) {
    mojo::ReportBadMessage(
        "Web Bluetooth is not allowed from an opaque origin.");
    return;
  }

  auto* impl = GetOrCreateForCurrentDocument(render_frame_host);
  if (!impl->Bind(std::move(receiver))) {
    // The renderer should only ever try to bind one instance of this service
    // per document.
    mojo::ReportBadMessage("Web Bluetooth already bound for current document.");
  }
}

WebBluetoothServiceImpl* WebBluetoothServiceImpl::CreateForTesting(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  WebBluetoothServiceImpl::BindIfAllowed(render_frame_host,
                                         std::move(receiver));
  return WebBluetoothServiceImpl::GetForCurrentDocument(render_frame_host);
}

DOCUMENT_USER_DATA_KEY_IMPL(WebBluetoothServiceImpl);

WebBluetoothServiceImpl::WebBluetoothServiceImpl(
    RenderFrameHost* render_frame_host)
    : DocumentUserData(render_frame_host),
      WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      receiver_(this),
      connected_devices_(new FrameConnectedBluetoothDevices(*render_frame_host))
#if PAIR_BLUETOOTH_ON_DEMAND()
      ,
      pairing_manager_(std::make_unique<WebBluetoothPairingManagerImpl>(this))
#endif
{
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents());

  BackForwardCache::DisableForRenderFrameHost(
      render_frame_host,
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kWebBluetooth));

  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (delegate) {
      observer_.Observe(delegate);
    }
  }
}

bool WebBluetoothServiceImpl::Bind(
    mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver) {
  if (receiver_.is_bound()) {
    return false;
  }
  receiver_.Bind(std::move(receiver));
  return true;
}

WebBluetoothServiceImpl::~WebBluetoothServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if PAIR_BLUETOOTH_ON_DEMAND()
  // Destroy the pairing manager before releasing the adapter to give it an
  // opportunity to cancel pairing operations that are in progress.
  pairing_manager_.reset();
#endif

  BluetoothAdapterFactoryWrapper::Get().ReleaseAdapter(this);

  // Force destructor of device_scanning_prompt_controller_ happening before
  // members destruction stage to prevent use-after-free accessing
  // scanning_clients_.empty().
  device_scanning_prompt_controller_.reset();
}

blink::mojom::WebBluetoothResult
WebBluetoothServiceImpl::GetBluetoothAllowed() {
  // The use of render_frame_host().GetMainFrame() below is safe as fenced
  // frames are disallowed.
  DCHECK(!render_frame_host().IsNestedWithinFencedFrame());

  BluetoothDelegate* delegate =
      GetContentClient()->browser()->GetBluetoothDelegate();
  if (!delegate || !delegate->MayUseBluetooth(&render_frame_host())) {
    return blink::mojom::WebBluetoothResult::WEB_BLUETOOTH_NOT_SUPPORTED;
  }

  // Check if Web Bluetooth is allowed by Permissions Policy.
  if (!render_frame_host().IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kBluetooth)) {
    return blink::mojom::WebBluetoothResult::PERMISSIONS_POLICY_VIOLATION;
  }

  const url::Origin& requesting_origin = origin();
  const url::Origin& embedding_origin =
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin();

  // Some embedders that don't support Web Bluetooth indicate this by not
  // returning a chooser.
  // TODO(crbug.com/41476036): Perform this check once there is a way to
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
  return GetWebBluetoothDeviceId(device_address).IsValid();
}

void WebBluetoothServiceImpl::OnBluetoothScanningPromptEvent(
    BluetoothScanningPrompt::Event event,
    BluetoothDeviceScanningPromptController* prompt_controller) {
  // The use of render_frame_host().GetMainFrame() below is safe as fenced
  // frames are disallowed.
  DCHECK(!render_frame_host().IsNestedWithinFencedFrame());

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
    const url::Origin requesting_origin = origin();
    const url::Origin embedding_origin =
        render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
    GetContentClient()->browser()->BlockBluetoothScanning(
        web_contents()->GetBrowserContext(), requesting_origin,
        embedding_origin);
  } else if (event == BluetoothScanningPrompt::Event::kCanceled) {
    result = blink::mojom::WebBluetoothResult::PROMPT_CANCELED;
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  client->RunCallback(std::move(result));
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
    NOTREACHED_IN_MIGRATION();
  }
}

void WebBluetoothServiceImpl::OnPermissionRevoked(const url::Origin& origin) {
  // The use of render_frame_host().GetMainFrame() below is safe as fenced
  // frames are disallowed.
  DCHECK(!render_frame_host().IsNestedWithinFencedFrame());

  if (render_frame_host().GetMainFrame()->GetLastCommittedOrigin() != origin) {
    return;
  }

  BluetoothDelegate* delegate =
      GetContentClient()->browser()->GetBluetoothDelegate();
  if (!delegate)
    return;

  std::set<blink::WebBluetoothDeviceId> permitted_ids;
  for (const auto& device : delegate->GetPermittedDevices(&render_frame_host()))
    permitted_ids.insert(device->id);

  connected_devices_->CloseConnectionsToDevicesNotInList(permitted_ids);

  std::erase_if(watch_advertisements_clients_,
                [&](const std::unique_ptr<WatchAdvertisementsClient>& client) {
                  return !base::Contains(permitted_ids, client->device_id());
                });

  MaybeStopDiscovery();
}

content::RenderFrameHost* WebBluetoothServiceImpl::GetRenderFrameHost() {
  return &render_frame_host();
}

void WebBluetoothServiceImpl::OnVisibilityChanged(Visibility visibility) {
  if (ShouldIgnoreVisibilityRequirementsForTesting()) {
    return;
  }

  if (visibility == content::Visibility::HIDDEN ||
      visibility == content::Visibility::OCCLUDED) {
    ClearAdvertisementClients();
  }
}

void WebBluetoothServiceImpl::OnWebContentsLostFocus(
    RenderWidgetHost* render_widget_host) {
  if (ShouldIgnoreVisibilityRequirementsForTesting()) {
    return;
  }

  ClearAdvertisementClients();
}

void WebBluetoothServiceImpl::AdapterPoweredChanged(BluetoothAdapter* adapter,
                                                    bool powered) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AdapterPoweredChanged(powered);
  }
}

void WebBluetoothServiceImpl::DeviceAdded(BluetoothAdapter* adapter,
                                          BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }
}

void WebBluetoothServiceImpl::DeviceChanged(BluetoothAdapter* adapter,
                                            BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  if (!device->IsGattConnected()) {
    std::optional<blink::WebBluetoothDeviceId> device_id =
        connected_devices_->CloseConnectionToDeviceWithAddress(
            device->GetAddress());

    // Since the device disconnected we need to send an error for pending
    // primary services requests.
    RunPendingPrimaryServicesRequests(device);
  }
}

void WebBluetoothServiceImpl::DeviceAdvertisementReceived(
    const std::string& device_address,
    const std::optional<std::string>& device_name,
    const std::optional<std::string>& advertisement_name,
    std::optional<int8_t> rssi,
    std::optional<int8_t> tx_power,
    std::optional<uint16_t> appearance,
    const BluetoothDevice::UUIDList& advertised_uuids,
    const BluetoothDevice::ServiceDataMap& service_data_map,
    const BluetoothDevice::ManufacturerDataMap& manufacturer_data_map) {
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
    device->id =
        delegate->AddScannedDevice(&render_frame_host(), device_address);
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

  std::vector<BluetoothUUID> uuids;
  for (auto& uuid : advertised_uuids)
    uuids.push_back(BluetoothUUID(uuid.canonical_value()));
  result->uuids = std::move(uuids);

  auto& manufacturer_data = result->manufacturer_data;
  for (const auto& entry : manufacturer_data_map) {
    manufacturer_data.emplace(
        blink::mojom::WebBluetoothCompany::New(entry.first), entry.second);
  }

  auto& service_data = result->service_data;
  service_data.insert(service_data_map.begin(), service_data_map.end());

  // TODO(crbug.com/40132791): These two classes can potentially be
  // combined into the same container.
  for (const auto& scanning_client : scanning_clients_)
    scanning_client->SendEvent(*result);

  for (const auto& watch_advertisements_client : watch_advertisements_clients_)
    watch_advertisements_client->SendEvent(*result);

  MaybeStopDiscovery();
}

void WebBluetoothServiceImpl::GattServicesDiscovered(BluetoothAdapter* adapter,
                                                     BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(1) << "Services discovered for device: " << device->GetAddress();

  if (device_chooser_controller_.get()) {
    device_chooser_controller_->AddFilteredDevice(*device);
  }

  RunPendingPrimaryServicesRequests(device);
}

void WebBluetoothServiceImpl::GattCharacteristicValueChanged(
    BluetoothAdapter* adapter,
    BluetoothRemoteGattCharacteristic* characteristic,
    const std::vector<uint8_t>& value) {
  // Don't notify of characteristics that we haven't returned.
  if (!base::Contains(characteristic_id_to_service_id_,
                      characteristic->GetIdentifier())) {
    return;
  }

  // On Chrome OS and Linux, GattCharacteristicValueChanged is called before the
  // success callback for ReadRemoteCharacteristic is called, which could result
  // in an event being fired before the readValue promise is resolved.
  if (!base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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
         scoped_refptr<BluetoothAdapter> adapter) {
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

void WebBluetoothServiceImpl::ForgetDevice(
    const blink::WebBluetoothDeviceId& device_id,
    ForgetDeviceCallback callback) {
  if (!base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    auto device_address = allowed_devices().GetDeviceAddress(device_id);
    // allowed_devices().RemoveDevice() expects a valid |device_address|.
    if (!device_address.empty()) {
      allowed_devices().RemoveDevice(device_address);
    }
    std::move(callback).Run();
    return;
  }

  BluetoothDelegate* delegate =
      GetContentClient()->browser()->GetBluetoothDelegate();
  if (delegate &&
      delegate->HasDevicePermission(&render_frame_host(), device_id)) {
    delegate->RevokeDevicePermissionWebInitiated(&render_frame_host(),
                                                 device_id);
  }
  std::move(callback).Run();
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
          delegate->HasDevicePermission(&render_frame_host(), device_id);
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

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
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
  mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient>
      web_bluetooth_server_client(std::move(client));

  query_result.device->CreateGattConnection(base::BindOnce(
      &WebBluetoothServiceImpl::OnCreateGATTConnection,
      weak_ptr_factory_.GetWeakPtr(), device_id,
      std::move(web_bluetooth_server_client), std::move(callback)));
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
    const std::optional<BluetoothUUID>& services_uuid,
    RemoteServerGetPrimaryServicesCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordGetPrimaryServicesServices(quantity, services_uuid);

  if (!IsAllowedToAccessAtLeastOneService(device_id)) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE,
        /*service=*/std::nullopt);
    return;
  }

  if (services_uuid &&
      !IsAllowedToAccessService(device_id, services_uuid.value())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_SERVICE,
        /*service=*/std::nullopt);
    return;
  }

  const CacheQueryResult query_result = QueryCacheForDevice(device_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    std::move(callback).Run(query_result.GetWebResult(),
                            std::nullopt /* service */);
    return;
  }

  RemoteServerGetPrimaryServicesImpl(device_id, quantity, services_uuid,
                                     std::move(callback), query_result.device);
}

void WebBluetoothServiceImpl::RemoteServiceGetCharacteristics(
    const std::string& service_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<BluetoothUUID>& characteristics_uuid,
    RemoteServiceGetCharacteristicsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RecordGetCharacteristicsCharacteristic(quantity, characteristics_uuid);

  if (characteristics_uuid &&
      BluetoothBlocklist::Get().IsExcluded(characteristics_uuid.value())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_CHARACTERISTIC_UUID,
        std::nullopt /* characteristics */);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForService(service_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    std::move(callback).Run(query_result.GetWebResult(),
                            std::nullopt /* characteristics */);
    return;
  }

  std::vector<BluetoothRemoteGattCharacteristic*> characteristics =
      characteristics_uuid ? query_result.service->GetCharacteristicsByUUID(
                                 characteristics_uuid.value())
                           : query_result.service->GetCharacteristics();

  std::vector<blink::mojom::WebBluetoothRemoteGATTCharacteristicPtr>
      response_characteristics;
  for (BluetoothRemoteGattCharacteristic* characteristic : characteristics) {
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
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_characteristics));
    return;
  }

  std::move(callback).Run(
      characteristics_uuid
          ? blink::mojom::WebBluetoothResult::CHARACTERISTIC_NOT_FOUND
          : blink::mojom::WebBluetoothResult::NO_CHARACTERISTICS_FOUND,
      std::nullopt /* characteristics */);
}

void WebBluetoothServiceImpl::RemoteCharacteristicGetDescriptors(
    const std::string& characteristic_instance_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<BluetoothUUID>& descriptors_uuid,
    RemoteCharacteristicGetDescriptorsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (descriptors_uuid &&
      BluetoothBlocklist::Get().IsExcluded(descriptors_uuid.value())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_DESCRIPTOR_UUID,
        std::nullopt /* descriptor */);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    std::move(callback).Run(query_result.GetWebResult(),
                            std::nullopt /* descriptor */);
    return;
  }

  auto descriptors = descriptors_uuid
                         ? query_result.characteristic->GetDescriptorsByUUID(
                               descriptors_uuid.value())
                         : query_result.characteristic->GetDescriptors();

  std::vector<blink::mojom::WebBluetoothRemoteGATTDescriptorPtr>
      response_descriptors;
  for (BluetoothRemoteGattDescriptor* descriptor : descriptors) {
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
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_descriptors));
    return;
  }
  std::move(callback).Run(
      descriptors_uuid ? blink::mojom::WebBluetoothResult::DESCRIPTOR_NOT_FOUND
                       : blink::mojom::WebBluetoothResult::NO_DESCRIPTORS_FOUND,
      std::nullopt /* descriptors */);
}

void WebBluetoothServiceImpl::RemoteCharacteristicReadValue(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicReadValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    RecordCharacteristicReadValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult(),
                            std::nullopt /* value */);
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromReads(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::kBlocklisted);
    std::move(callback).Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_READ,
                            std::nullopt /* value */);
    return;
  }

  query_result.characteristic->ReadRemoteCharacteristic(
      base::BindOnce(&WebBluetoothServiceImpl::OnCharacteristicReadValue,
                     weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
                     std::move(callback)));
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
    ReceivedBadMessage(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    RecordCharacteristicWriteValueOutcome(query_result.outcome);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromWrites(
          query_result.characteristic->GetUUID())) {
    RecordCharacteristicWriteValueOutcome(
        UMAGATTOperationOutcome::kBlocklisted);
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_WRITE);
    return;
  }

  // TODO(crbug.com/40524549): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  base::OnceClosure write_callback = base::BindOnce(
      &WebBluetoothServiceImpl::OnCharacteristicWriteValueSuccess,
      weak_ptr_factory_.GetWeakPtr(), std::move(split_callback.first));
  BluetoothGattCharacteristic::ErrorCallback write_error_callback =
      base::BindOnce(&WebBluetoothServiceImpl::OnCharacteristicWriteValueFailed,
                     weak_ptr_factory_.GetWeakPtr(), characteristic_instance_id,
                     value, write_type, std::move(split_callback.second));
  using WebBluetoothWriteType = blink::mojom::WebBluetoothWriteType;
  using WriteType = BluetoothRemoteGattCharacteristic::WriteType;
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

void WebBluetoothServiceImpl::RemoteCharacteristicStartNotificationsInternal(
    const std::string& characteristic_instance_id,
    mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
        client,
    RemoteCharacteristicStartNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);
  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    RecordStartNotificationsOutcome(UMAGATTOperationOutcome::kNotSupported);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  characteristic_id_to_notify_session_[characteristic_instance_id] =
      std::make_unique<GATTNotifySessionAndCharacteristicClient>(
          std::move(client));

  // TODO(crbug.com/40524549): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  query_result.characteristic->StartNotifySession(
      base::BindOnce(&WebBluetoothServiceImpl::OnStartNotifySessionSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&WebBluetoothServiceImpl::OnStartNotifySessionFailed,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.second),
                     characteristic_instance_id));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStartNotifications(
    const std::string& characteristic_instance_id,
    mojo::PendingAssociatedRemote<
        blink::mojom::WebBluetoothCharacteristicClient> client,
    RemoteCharacteristicStartNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  if (iter != characteristic_id_to_notify_session_.end()) {
    const auto& notification_client = iter->second;
    if (!notification_client->gatt_notify_session) {
      // There is an in-flight startNotification being processed which is
      // awaiting a notify session. Defer this start, and continue once the
      // in-flight start has completed.
      characteristic_id_to_deferred_start_[characteristic_instance_id].emplace(
          std::make_unique<DeferredStartNotificationData>(std::move(client),
                                                          std::move(callback)));
      return;
    }
    if (notification_client->gatt_notify_session->IsActive()) {
      // If the frame has already started notifications and the notifications
      // are active we return SUCCESS.
      std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
      return;
    }
  }

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    RecordStartNotificationsOutcome(UMAGATTOperationOutcome::kNotSupported);
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  BluetoothRemoteGattCharacteristic::Properties notify_or_indicate =
      query_result.characteristic->GetProperties() &
      (BluetoothRemoteGattCharacteristic::PROPERTY_NOTIFY |
       BluetoothRemoteGattCharacteristic::PROPERTY_INDICATE);
  if (!notify_or_indicate) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::GATT_NOT_SUPPORTED);
    return;
  }

  // Create entry in the notify session map - even before the notification
  // is successfully registered. This allows clients to send value change
  // notifications during the notification registration process.
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
      characteristic_client(std::move(client));

  RemoteCharacteristicStartNotificationsInternal(
      characteristic_instance_id, std::move(characteristic_client),
      std::move(callback));
}

void WebBluetoothServiceImpl::RemoteCharacteristicStopNotifications(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicStopNotificationsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const CacheQueryResult query_result =
      QueryCacheForCharacteristic(characteristic_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
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

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    std::move(callback).Run(query_result.GetWebResult(),
                            std::nullopt /* value */);
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromReads(
          query_result.descriptor->GetUUID())) {
    std::move(callback).Run(blink::mojom::WebBluetoothResult::BLOCKLISTED_READ,
                            std::nullopt /* value */);
    return;
  }

  query_result.descriptor->ReadRemoteDescriptor(
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorReadValue,
                     weak_ptr_factory_.GetWeakPtr(), descriptor_instance_id,
                     std::move(callback)));
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
    ReceivedBadMessage(bad_message::BDH_INVALID_WRITE_VALUE_LENGTH);
    return;
  }

  const CacheQueryResult query_result =
      QueryCacheForDescriptor(descriptor_instance_id);

  if (query_result.outcome == CacheQueryOutcome::kBadRenderer) {
    return;
  }

  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    std::move(callback).Run(query_result.GetWebResult());
    return;
  }

  if (BluetoothBlocklist::Get().IsExcludedFromWrites(
          query_result.descriptor->GetUUID())) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLOCKLISTED_WRITE);
    return;
  }

  // TODO(crbug.com/40524549): Remove SplitOnceCallback() by updating
  // the callee interface.
  auto split_callback = base::SplitOnceCallback(std::move(callback));
  query_result.descriptor->WriteRemoteDescriptor(
      value,
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorWriteValueSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_callback.first)),
      base::BindOnce(&WebBluetoothServiceImpl::OnDescriptorWriteValueFailed,
                     weak_ptr_factory_.GetWeakPtr(), descriptor_instance_id,
                     value, std::move(split_callback.second)));
}

void WebBluetoothServiceImpl::RequestScanningStart(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // The use of render_frame_host().GetMainFrame() below is safe as fenced
  // frames are disallowed.
  DCHECK(!render_frame_host().IsNestedWithinFencedFrame());

  const url::Origin requesting_origin = origin();
  const url::Origin embedding_origin =
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin();

  bool blocked = GetContentClient()->browser()->IsBluetoothScanningBlocked(
      web_contents()->GetBrowserContext(), requesting_origin, embedding_origin);
  if (blocked) {
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SCANNING_BLOCKED);
    return;
  }

  // The renderer should never send invalid options.
  if (!IsValidRequestScanOptions(options)) {
    ReceivedBadMessage(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this, base::BindOnce(
                    &WebBluetoothServiceImpl::RequestScanningStartImpl,
                    weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
                    std::move(options), std::move(callback)));
      return;
    }
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  RequestScanningStartImpl(std::move(client_remote), std::move(options),
                           std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::WatchAdvertisementsForDevice(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    WatchAdvertisementsForDeviceCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  blink::mojom::WebBluetoothResult allowed_result = GetBluetoothAllowed();
  if (allowed_result != blink::mojom::WebBluetoothResult::SUCCESS) {
    std::move(callback).Run(allowed_result);
    return;
  }

  // The renderer should never send an invalid |device_id|.
  if (!device_id.IsValid()) {
    ReceivedBadMessage(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  if (!GetAdapter()) {
    if (BluetoothAdapterFactoryWrapper::Get().IsLowEnergySupported()) {
      BluetoothAdapterFactoryWrapper::Get().AcquireAdapter(
          this, base::BindOnce(
                    &WebBluetoothServiceImpl::WatchAdvertisementsForDeviceImpl,
                    weak_ptr_factory_.GetWeakPtr(), device_id,
                    std::move(client_remote), std::move(callback)));
      return;
    }
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  WatchAdvertisementsForDeviceImpl(std::move(device_id),
                                   std::move(client_remote),
                                   std::move(callback), GetAdapter());
}

void WebBluetoothServiceImpl::RemoveDisconnectedClients() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(crbug.com/40132791): These two classes can potentially be
  // combined into the same container.
  std::erase_if(scanning_clients_,
                [](const std::unique_ptr<ScanningClient>& client) {
                  return !client->is_connected();
                });
  std::erase_if(watch_advertisements_clients_,
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
        client_remote,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    RequestScanningStartCallback callback,
    scoped_refptr<BluetoothAdapter> adapter) {
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
        /*service=*/this, std::move(client_remote), std::move(options),
        std::move(callback));

    if (AreScanFiltersAllowed(scanning_client->scan_options().filters)) {
      scanning_client->RunCallback(blink::mojom::WebBluetoothResult::SUCCESS);
      scanning_client->set_allow_send_event(true);
      scanning_clients_.push_back(std::move(scanning_client));
      return;
    }

    // By resetting |device_scanning_prompt_controller_|, it returns an error if
    // there are duplicate calls to RequestScanningStart().
    device_scanning_prompt_controller_ =
        std::make_unique<BluetoothDeviceScanningPromptController>(
            this, render_frame_host());
    scanning_client->SetPromptController(
        device_scanning_prompt_controller_.get());
    scanning_clients_.push_back(std::move(scanning_client));
    device_scanning_prompt_controller_->ShowPermissionPrompt();
    return;
  }

  request_scanning_start_callback_ = std::move(callback);

  // TODO(crbug.com/40630111): Since scanning without a filter wastes
  // resources, we need use StartDiscoverySessionWithFilter() instead of
  // StartDiscoverySession() here.
  adapter->StartDiscoverySession(
      kScanClientNameRequestLeScan,
      base::BindOnce(
          &WebBluetoothServiceImpl::OnStartDiscoverySessionForScanning,
          weak_ptr_factory_.GetWeakPtr(), std::move(client_remote),
          std::move(options)),
      base::BindOnce(
          &WebBluetoothServiceImpl::OnDiscoverySessionErrorForScanning,
          weak_ptr_factory_.GetWeakPtr()));
}

void WebBluetoothServiceImpl::OnStartDiscoverySessionForScanning(
    mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothAdvertisementClient>
        client_remote,
    blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
    std::unique_ptr<BluetoothDiscoverySession> session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!ble_scan_discovery_session_);

  ble_scan_discovery_session_ = std::move(session);

  auto scanning_client = std::make_unique<ScanningClient>(
      /*service=*/this, std::move(client_remote), std::move(options),
      std::move(request_scanning_start_callback_));

  if (AreScanFiltersAllowed(scanning_client->scan_options().filters)) {
    scanning_client->RunCallback(blink::mojom::WebBluetoothResult::SUCCESS);
    scanning_client->set_allow_send_event(true);
    scanning_clients_.push_back(std::move(scanning_client));
    return;
  }

  device_scanning_prompt_controller_ =
      std::make_unique<BluetoothDeviceScanningPromptController>(
          this, render_frame_host());
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
    scoped_refptr<BluetoothAdapter> adapter) {
  // The renderer should never send invalid options.
  if (!IsValidRequestDeviceOptions(options)) {
    ReceivedBadMessage(bad_message::BDH_INVALID_OPTIONS);
    return;
  }

  // Calls to requestDevice() require user activation (user gestures).  We
  // should close any opened chooser when a duplicate requestDevice call is
  // made with the same user activation or when any gesture occurs outside
  // of the opened chooser. This does not happen on all platforms so we
  // don't DCHECK that the old one is closed.  We destroy the old chooser
  // before constructing the new one to make sure they can't conflict.
  device_chooser_controller_.reset();

  device_chooser_controller_ =
      std::make_unique<BluetoothDeviceChooserController>(
          this, render_frame_host(), std::move(adapter));
  device_chooser_controller_->GetDevice(
      std::move(options),
      base::BindOnce(&WebBluetoothServiceImpl::OnGetDevice,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebBluetoothServiceImpl::GetDevicesImpl(
    GetDevicesCallback callback,
    scoped_refptr<BluetoothAdapter> adapter) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate) {
      std::move(callback).Run({});
      return;
    }

    std::move(callback).Run(
        delegate->GetPermittedDevices(&render_frame_host()));
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
        client_remote,
    WatchAdvertisementsForDeviceCallback callback,
    scoped_refptr<BluetoothAdapter> adapter) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!adapter) {
    std::move(callback).Run(
        blink::mojom::WebBluetoothResult::BLUETOOTH_LOW_ENERGY_NOT_AVAILABLE);
    return;
  }

  auto pending_client = std::make_unique<WatchAdvertisementsClient>(
      /*service=*/this, std::move(client_remote), std::move(device_id),
      std::move(callback));
  if (watch_advertisements_discovery_session_) {
    pending_client->RunCallback(blink::mojom::WebBluetoothResult::SUCCESS);
    watch_advertisements_clients_.push_back(std::move(pending_client));
    return;
  }

  // If |watch_advertisements_pending_clients_| has more than one client,
  // then it means that a previous watch advertisements operation has already
  // started a discovery session, so the |pending_client| for this
  // operation needs to be stored until the start discovery operation is
  // complete.
  watch_advertisements_pending_clients_.push_back(std::move(pending_client));
  if (watch_advertisements_pending_clients_.size() > 1) {
    return;
  }

  // Not all platforms support filtering by address.
  // TODO(crbug.com/40630111): Use StartDiscoverySessionWithFilter() to
  // filter out by MAC address when platforms provide this capability.
  adapter->StartDiscoverySession(
      kScanClientNameWatchAdvertisements,
      base::BindOnce(&WebBluetoothServiceImpl::
                         OnStartDiscoverySessionForWatchAdvertisements,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&WebBluetoothServiceImpl::
                         OnDiscoverySessionErrorForWatchAdvertisements,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebBluetoothServiceImpl::OnStartDiscoverySessionForWatchAdvertisements(
    std::unique_ptr<BluetoothDiscoverySession> session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!watch_advertisements_discovery_session_);
  watch_advertisements_discovery_session_ = std::move(session);

  BluetoothDelegate* delegate =
      GetContentClient()->browser()->GetBluetoothDelegate();

  for (auto& pending_client : watch_advertisements_pending_clients_) {
    // Check if |pending_client| is still alive.
    if (!pending_client->is_connected()) {
      pending_client->RunCallback(
          blink::mojom::WebBluetoothResult::WATCH_ADVERTISEMENTS_ABORTED);
      continue;
    }

    // If the new permissions backend is enabled, verify the permission using
    // the delegate.
    if (base::FeatureList::IsEnabled(
            features::kWebBluetoothNewPermissionsBackend) &&
        (!delegate || !delegate->HasDevicePermission(
                          &render_frame_host(), pending_client->device_id()))) {
      pending_client->RunCallback(
          blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE);
      continue;
    }

    // Otherwise verify it via |allowed_devices|.
    if (!base::FeatureList::IsEnabled(
            features::kWebBluetoothNewPermissionsBackend) &&
        !allowed_devices().IsAllowedToGATTConnect(
            pending_client->device_id())) {
      pending_client->RunCallback(
          blink::mojom::WebBluetoothResult::NOT_ALLOWED_TO_ACCESS_ANY_SERVICE);
      continue;
    }

    // Here we already make sure that pending_client is still alive and have
    // permissions. Add it to |watch_advertisements_clients_|.
    pending_client->RunCallback(blink::mojom::WebBluetoothResult::SUCCESS);
    watch_advertisements_clients_.push_back(std::move(pending_client));
  }

  watch_advertisements_pending_clients_.clear();

  // If a client was disconnected while a discovery session was being started,
  // then there may not be any valid clients, so discovery should be stopped.
  MaybeStopDiscovery();
}

void WebBluetoothServiceImpl::OnDiscoverySessionErrorForWatchAdvertisements() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (auto& pending_client : watch_advertisements_pending_clients_) {
    pending_client->RunCallback(
        blink::mojom::WebBluetoothResult::NO_BLUETOOTH_ADAPTER);
  }
  watch_advertisements_pending_clients_.clear();
  ClearAdvertisementClients();
}

void WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl(
    const blink::WebBluetoothDeviceId& device_id,
    blink::mojom::WebBluetoothGATTQueryQuantity quantity,
    const std::optional<BluetoothUUID>& services_uuid,
    RemoteServerGetPrimaryServicesCallback callback,
    BluetoothDevice* device) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!device->IsGattConnected()) {
    // The device disconnected while discovery was pending. The returned error
    // does not matter because the renderer ignores the error if the device
    // disconnected.
    std::move(callback).Run(blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND,
                            std::nullopt /* services */);
    return;
  }

  // We can't know if a service is present or not until GATT service discovery
  // is complete for the device.
  if (!device->IsGattServicesDiscoveryComplete()) {
    DVLOG(1) << "Services not yet discovered.";
    pending_primary_services_requests_[device->GetAddress()].push_back(
        base::BindOnce(
            &WebBluetoothServiceImpl::RemoteServerGetPrimaryServicesImpl,
            base::Unretained(this), device_id, quantity, services_uuid,
            std::move(callback)));
    return;
  }

  DCHECK(device->IsGattServicesDiscoveryComplete());

  std::vector<BluetoothRemoteGattService*> services =
      services_uuid ? device->GetPrimaryServicesByUUID(services_uuid.value())
                    : device->GetPrimaryServices();

  std::vector<blink::mojom::WebBluetoothRemoteGATTServicePtr> response_services;
  for (BluetoothRemoteGattService* service : services) {
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
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                            std::move(response_services));
    return;
  }

  DVLOG(1) << "Services not found in device.";
  std::move(callback).Run(
      services_uuid ? blink::mojom::WebBluetoothResult::SERVICE_NOT_FOUND
                    : blink::mojom::WebBluetoothResult::NO_SERVICES_FOUND,
      std::nullopt /* services */);
}

void WebBluetoothServiceImpl::OnGetDevice(
    RequestDeviceCallback callback,
    blink::mojom::WebBluetoothResult result,
    blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
    const std::string& device_address) {
  device_chooser_controller_.reset();

  if (result != blink::mojom::WebBluetoothResult::SUCCESS) {
    // Errors are recorded by |device_chooser_controller_|.
    std::move(callback).Run(result, /*device=*/nullptr);
    return;
  }

  const BluetoothDevice* const device = GetAdapter()->GetDevice(device_address);
  if (device == nullptr) {
    DVLOG(1) << "Device " << device_address << " no longer in adapter";
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
        &render_frame_host(), device, options.get());
  } else {
    web_bluetooth_device->id =
        allowed_devices().AddDevice(device_address, options);
  }
  web_bluetooth_device->name = device->GetName();

  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS,
                          std::move(web_bluetooth_device));
}

void WebBluetoothServiceImpl::OnCreateGATTConnection(
    const blink::WebBluetoothDeviceId& device_id,
    mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client,
    RemoteServerConnectCallback callback,
    std::unique_ptr<BluetoothGattConnection> connection,
    std::optional<BluetoothDevice::ConnectErrorCode> error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error_code.has_value()) {
    std::move(callback).Run(TranslateConnectErrorAndRecord(error_code.value()));
    return;
  }
  RecordConnectGATTOutcome(UMAConnectGATTOutcome::kSuccess);

  if (connected_devices_->IsConnectedToDeviceWithId(device_id)) {
    DVLOG(1) << "Already connected.";
    std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
    return;
  }

  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
  connected_devices_->Insert(device_id, std::move(connection),
                             std::move(client));
}

void WebBluetoothServiceImpl::OnCharacteristicReadValue(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicReadValueCallback callback,
    std::optional<GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error_code.has_value()) {
#if PAIR_BLUETOOTH_ON_DEMAND()
    if (error_code.value() == GattErrorCode::kNotAuthorized ||
        error_code.value() == GattErrorCode::kNotPaired) {
      BluetoothDevice* device = GetCachedDevice(
          GetCharacteristicDeviceID(characteristic_instance_id));
      if (device && !device->IsPaired()) {
        // Initiate pairing. See (Secure Characteristics) in README.md for more
        // information.
        pairing_manager_->PairForCharacteristicReadValue(
            characteristic_instance_id, std::move(callback));
        return;
      }
    }
#endif  // PAIR_BLUETOOTH_ON_DEMAND()
    std::move(callback).Run(
        TranslateGATTErrorAndRecord(error_code.value(),
                                    UMAGATTOperation::kCharacteristicRead),
        /*value=*/std::nullopt);
    return;
  }
  RecordCharacteristicReadValueOutcome(UMAGATTOperationOutcome::kSuccess);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS, value);
}

void WebBluetoothServiceImpl::OnCharacteristicWriteValueSuccess(
    RemoteCharacteristicWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RecordCharacteristicWriteValueOutcome(UMAGATTOperationOutcome::kSuccess);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnCharacteristicWriteValueFailed(
    const std::string& characteristic_instance_id,
    const std::vector<uint8_t>& value,
    blink::mojom::WebBluetoothWriteType write_type,
    RemoteCharacteristicWriteValueCallback callback,
    GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if PAIR_BLUETOOTH_ON_DEMAND()
  if (error_code == GattErrorCode::kNotAuthorized) {
    BluetoothDevice* device =
        GetCachedDevice(GetCharacteristicDeviceID(characteristic_instance_id));
    if (device && !device->IsPaired()) {
      // Initiate pairing. See (Secure Characteristics) in README.md for more
      // information.
      pairing_manager_->PairForCharacteristicWriteValue(
          characteristic_instance_id, value, write_type, std::move(callback));
      return;
    }
  }
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::kCharacteristicWrite));
}

void WebBluetoothServiceImpl::OnStartNotifySessionSuccess(
    RemoteCharacteristicStartNotificationsCallback callback,
    std::unique_ptr<BluetoothGattNotifySession> notify_session) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
  std::string characteristic_id = notify_session->GetCharacteristicIdentifier();
  auto iter = characteristic_id_to_notify_session_.find(characteristic_id);

  if (iter == characteristic_id_to_notify_session_.end())
    return;
  // Saving the BluetoothGattNotifySession keeps notifications active.
  iter->second->gatt_notify_session = std::move(notify_session);

  // Continue any deferred notification starts.
  auto deferred_iter =
      characteristic_id_to_deferred_start_.find(characteristic_id);
  if (deferred_iter != characteristic_id_to_deferred_start_.end()) {
    base::queue<std::unique_ptr<DeferredStartNotificationData>> deferral_queue =
        std::move(deferred_iter->second);
    characteristic_id_to_deferred_start_.erase(deferred_iter);
    while (!deferral_queue.empty()) {
      RemoteCharacteristicStartNotifications(
          characteristic_id, std::move(deferral_queue.front()->client),
          std::move(deferral_queue.front()->callback));
      deferral_queue.pop();
    }
  }
}

void WebBluetoothServiceImpl::OnStartNotifySessionFailed(
    RemoteCharacteristicStartNotificationsCallback callback,
    const std::string& characteristic_instance_id,
    GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter =
      characteristic_id_to_notify_session_.find(characteristic_instance_id);
  mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient> client;
  if (iter != characteristic_id_to_notify_session_.end()) {
    client = std::move(iter->second->characteristic_client);
    characteristic_id_to_notify_session_.erase(iter);
  }

#if PAIR_BLUETOOTH_ON_DEMAND()
  if (error_code == GattErrorCode::kNotAuthorized && client) {
    BluetoothDevice* device =
        GetCachedDevice(GetCharacteristicDeviceID(characteristic_instance_id));
    if (device && !device->IsPaired()) {
      // Initiate pairing. See (Secure Characteristics) in README.md for more
      // information.
      pairing_manager_->PairForCharacteristicStartNotifications(
          characteristic_instance_id, std::move(client), std::move(callback));
      return;
    }
  }
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::kStartNotifications));

  // Fail any deferred notification starts blocked on this one.
  auto deferred_iter =
      characteristic_id_to_deferred_start_.find(characteristic_instance_id);
  if (deferred_iter != characteristic_id_to_deferred_start_.end()) {
    base::queue<std::unique_ptr<DeferredStartNotificationData>> deferral_queue =
        std::move(deferred_iter->second);
    characteristic_id_to_deferred_start_.erase(deferred_iter);
    while (!deferral_queue.empty()) {
      // Run paused start callbacks with the same error code that caused the
      // first one to fail.
      std::move(deferral_queue.front()->callback)
          .Run(TranslateGATTErrorAndRecord(
              error_code, UMAGATTOperation::kStartNotifications));
      deferral_queue.pop();
    }
  }
}

void WebBluetoothServiceImpl::OnStopNotifySessionComplete(
    const std::string& characteristic_instance_id,
    RemoteCharacteristicStopNotificationsCallback callback) {
  characteristic_id_to_notify_session_.erase(characteristic_instance_id);
  std::move(callback).Run();
}

void WebBluetoothServiceImpl::OnDescriptorReadValue(
    const std::string& descriptor_instance_id,
    RemoteDescriptorReadValueCallback callback,
    std::optional<GattErrorCode> error_code,
    const std::vector<uint8_t>& value) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (error_code.has_value()) {
#if PAIR_BLUETOOTH_ON_DEMAND()
    if (error_code.value() == GattErrorCode::kNotAuthorized) {
      BluetoothDevice* device =
          GetCachedDevice(GetDescriptorDeviceId(descriptor_instance_id));
      if (device && !device->IsPaired()) {
        // Initiate pairing. See (Secure Characteristics) in README.md for more
        // information.
        pairing_manager_->PairForDescriptorReadValue(descriptor_instance_id,
                                                     std::move(callback));
        return;
      }
    }
#endif  // PAIR_BLUETOOTH_ON_DEMAND()
    std::move(callback).Run(
        TranslateGATTErrorAndRecord(error_code.value(),
                                    UMAGATTOperation::kDescriptorReadObsolete),
        /*value=*/std::nullopt);
    return;
  }
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS, value);
}

void WebBluetoothServiceImpl::OnDescriptorWriteValueSuccess(
    RemoteDescriptorWriteValueCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(blink::mojom::WebBluetoothResult::SUCCESS);
}

void WebBluetoothServiceImpl::OnDescriptorWriteValueFailed(
    const std::string& descriptor_instance_id,
    const std::vector<uint8_t>& value,
    RemoteDescriptorWriteValueCallback callback,
    GattErrorCode error_code) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

#if PAIR_BLUETOOTH_ON_DEMAND()
  if (error_code == GattErrorCode::kNotAuthorized) {
    BluetoothDevice* device =
        GetCachedDevice(GetDescriptorDeviceId(descriptor_instance_id));
    if (device && !device->IsPaired()) {
      // Initiate pairing. See (Secure Characteristics) in README.md for more
      // information.
      pairing_manager_->PairForDescriptorWriteValue(descriptor_instance_id,
                                                    value, std::move(callback));
      return;
    }
  }
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

  std::move(callback).Run(TranslateGATTErrorAndRecord(
      error_code, UMAGATTOperation::kDescriptorWriteObsolete));
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
          delegate->GetDeviceAddress(&render_frame_host(), device_id);
    }
  } else {
    device_address = allowed_devices().GetDeviceAddress(device_id);
  }

  if (device_address.empty()) {
    ReceivedBadMessage(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }

  CacheQueryResult result;
  result.device = GetAdapter()->GetDevice(device_address);

  // When a device can't be found in the BluetoothAdapter, that generally
  // indicates that it's gone out of range. We reject with a NetworkError in
  // that case.
  if (result.device == nullptr) {
    result.outcome = CacheQueryOutcome::kNoDevice;
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForService(
    const std::string& service_instance_id) {
  auto device_iter = service_id_to_device_address_.find(service_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (device_iter == service_id_to_device_address_.end()) {
    ReceivedBadMessage(bad_message::BDH_INVALID_SERVICE_ID);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }

  const blink::WebBluetoothDeviceId device_id =
      GetWebBluetoothDeviceId(device_iter->second);

  // Kill the renderer if origin is not allowed to access the device.
  if (!device_id.IsValid()) {
    ReceivedBadMessage(bad_message::BDH_DEVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }

  CacheQueryResult result = QueryCacheForDevice(device_id);
  if (result.outcome != CacheQueryOutcome::kSuccess) {
    return result;
  }

  result.service = result.device->GetGattService(service_instance_id);
  if (result.service == nullptr) {
    result.outcome = CacheQueryOutcome::kNoService;
    return result;
  }

  if (!IsAllowedToAccessService(device_id, result.service->GetUUID())) {
    ReceivedBadMessage(bad_message::BDH_SERVICE_NOT_ALLOWED_FOR_ORIGIN);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }
  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForCharacteristic(
    const std::string& characteristic_instance_id) {
  auto characteristic_iter =
      characteristic_id_to_service_id_.find(characteristic_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (characteristic_iter == characteristic_id_to_service_id_.end()) {
    ReceivedBadMessage(bad_message::BDH_INVALID_CHARACTERISTIC_ID);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }

  CacheQueryResult result = QueryCacheForService(characteristic_iter->second);

  if (result.outcome != CacheQueryOutcome::kSuccess) {
    return result;
  }

  result.characteristic =
      result.service->GetCharacteristic(characteristic_instance_id);

  if (result.characteristic == nullptr) {
    result.outcome = CacheQueryOutcome::kNoCharacteristic;
  }

  return result;
}

CacheQueryResult WebBluetoothServiceImpl::QueryCacheForDescriptor(
    const std::string& descriptor_instance_id) {
  auto descriptor_iter =
      descriptor_id_to_characteristic_id_.find(descriptor_instance_id);

  // Kill the render, see "ID Not in Map Note" above.
  if (descriptor_iter == descriptor_id_to_characteristic_id_.end()) {
    ReceivedBadMessage(bad_message::BDH_INVALID_DESCRIPTOR_ID);
    return CacheQueryResult(CacheQueryOutcome::kBadRenderer);
  }

  CacheQueryResult result =
      QueryCacheForCharacteristic(descriptor_iter->second);

  if (result.outcome != CacheQueryOutcome::kSuccess) {
    return result;
  }

  result.descriptor =
      result.characteristic->GetDescriptor(descriptor_instance_id);

  if (result.descriptor == nullptr) {
    result.outcome = CacheQueryOutcome::kNoDescriptor;
  }

  return result;
}

void WebBluetoothServiceImpl::RunPendingPrimaryServicesRequests(
    BluetoothDevice* device) {
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
  return render_frame_host().GetProcess();
}

BluetoothAdapter* WebBluetoothServiceImpl::GetAdapter() {
  return BluetoothAdapterFactoryWrapper::Get().GetAdapter(this);
}

void WebBluetoothServiceImpl::ReceivedBadMessage(
    bad_message::BadMessageReason reason) {
  bad_message::ReceivedBadMessage(GetRenderProcessHost(), reason);
  // Ideally, this would use receiver_.ReportBadMessage(), but for legacy
  // reasons, the Bluetooth service code uses the BadMessageReason enum, which
  // is incompatible.
  receiver_.reset();
}

BluetoothAllowedDevices& WebBluetoothServiceImpl::allowed_devices() {
  // We should use the embedding origin so that permission delegation using
  // Permissions Policy works correctly.
  const url::Origin& embedding_origin =
      render_frame_host().GetMainFrame()->GetLastCommittedOrigin();
  StoragePartitionImpl* partition = static_cast<StoragePartitionImpl*>(
      web_contents()->GetBrowserContext()->GetDefaultStoragePartition());
  return partition->GetBluetoothAllowedDevicesMap()->GetOrCreateAllowedDevices(
      embedding_origin);
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
    const std::optional<ScanFilters>& filters) const {
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
    return delegate->IsAllowedToAccessAtLeastOneService(&render_frame_host(),
                                                        device_id);
  }
  return allowed_devices().IsAllowedToAccessAtLeastOneService(device_id);
}

bool WebBluetoothServiceImpl::IsAllowedToAccessService(
    const blink::WebBluetoothDeviceId& device_id,
    const BluetoothUUID& service) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return false;
    return delegate->IsAllowedToAccessService(&render_frame_host(), device_id,
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
        &render_frame_host(), device_id, manufacturer_code);
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

blink::WebBluetoothDeviceId WebBluetoothServiceImpl::GetCharacteristicDeviceID(
    const std::string& characteristic_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto characteristic_iter =
      characteristic_id_to_service_id_.find(characteristic_instance_id);
  if (characteristic_iter == characteristic_id_to_service_id_.end())
    return blink::WebBluetoothDeviceId();
  auto device_iter =
      service_id_to_device_address_.find(characteristic_iter->second);
  if (device_iter == service_id_to_device_address_.end())
    return blink::WebBluetoothDeviceId();

  return GetWebBluetoothDeviceId(device_iter->second);
}

BluetoothDevice* WebBluetoothServiceImpl::GetCachedDevice(
    const blink::WebBluetoothDeviceId& device_id) {
  DCHECK(device_id.IsValid());
  CacheQueryResult query_result = QueryCacheForDevice(device_id);
  if (query_result.outcome != CacheQueryOutcome::kSuccess) {
    return nullptr;
  }

  return query_result.device;
}

blink::WebBluetoothDeviceId WebBluetoothServiceImpl::GetDescriptorDeviceId(
    const std::string& descriptor_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto iter = descriptor_id_to_characteristic_id_.find(descriptor_instance_id);
  if (iter == descriptor_id_to_characteristic_id_.end())
    return blink::WebBluetoothDeviceId();

  return GetCharacteristicDeviceID(iter->second);
}

blink::WebBluetoothDeviceId WebBluetoothServiceImpl::GetWebBluetoothDeviceId(
    const std::string& device_address) {
  if (base::FeatureList::IsEnabled(
          features::kWebBluetoothNewPermissionsBackend)) {
    BluetoothDelegate* delegate =
        GetContentClient()->browser()->GetBluetoothDelegate();
    if (!delegate)
      return blink::WebBluetoothDeviceId();
    return delegate->GetWebBluetoothDeviceId(&render_frame_host(),
                                             device_address);
  }

  const blink::WebBluetoothDeviceId* device_id_ptr =
      allowed_devices().GetDeviceId(device_address);
  return device_id_ptr ? *device_id_ptr : blink::WebBluetoothDeviceId();
}

void WebBluetoothServiceImpl::PairDevice(
    const blink::WebBluetoothDeviceId& device_id,
    BluetoothDevice::PairingDelegate* pairing_delegate,
    BluetoothDevice::ConnectCallback callback) {
  if (!device_id.IsValid()) {
    std::move(callback).Run(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  BluetoothDevice* device = GetCachedDevice(device_id);
  if (!device) {
    std::move(callback).Run(BluetoothDevice::ConnectErrorCode::ERROR_UNKNOWN);
    return;
  }

  DCHECK(!device->IsPaired());

  device->Pair(pairing_delegate, std::move(callback));
}

void WebBluetoothServiceImpl::CancelPairing(
    const blink::WebBluetoothDeviceId& device_id) {
  DCHECK(device_id.IsValid());

  BluetoothDevice* device = GetCachedDevice(device_id);
  if (!device)
    return;

  device->CancelPairing();
}

void WebBluetoothServiceImpl::SetPinCode(
    const blink::WebBluetoothDeviceId& device_id,
    const std::string& pincode) {
  DCHECK(device_id.IsValid());

  BluetoothDevice* device = GetCachedDevice(device_id);
  if (!device)
    return;

  device->SetPinCode(pincode);
}

void WebBluetoothServiceImpl::PairConfirmed(
    const blink::WebBluetoothDeviceId& device_id) {
  DCHECK(device_id.IsValid());

  BluetoothDevice* device = GetCachedDevice(device_id);
  if (!device)
    return;

  device->ConfirmPairing();
}

void WebBluetoothServiceImpl::PromptForBluetoothPairing(
    const std::u16string& device_identifier,
    BluetoothDelegate::PairPromptCallback callback,
    BluetoothDelegate::PairingKind pairing_kind,
    const std::optional<std::u16string>& pin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BluetoothDelegate* delegate =
      GetContentClient()->browser()->GetBluetoothDelegate();

  if (!delegate) {
    std::move(callback).Run(BluetoothDelegate::PairPromptResult(
        BluetoothDelegate::PairPromptStatus::kCancelled));
    return;
  }

  switch (pairing_kind) {
    case BluetoothDelegate::PairingKind::kConfirmOnly:
    case BluetoothDelegate::PairingKind::kProvidePin:
    case BluetoothDelegate::PairingKind::kConfirmPinMatch:
      delegate->ShowDevicePairPrompt(&render_frame_host(), device_identifier,
                                     std::move(callback), pairing_kind, pin);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      std::move(callback).Run(BluetoothDelegate::PairPromptResult(
          BluetoothDelegate::PairPromptStatus::kCancelled));
      break;
  }
}

#if PAIR_BLUETOOTH_ON_DEMAND()
void WebBluetoothServiceImpl::SetPairingManagerForTesting(
    std::unique_ptr<WebBluetoothPairingManager> pairing_manager) {
  pairing_manager_ = std::move(pairing_manager);
}
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

}  // namespace content
