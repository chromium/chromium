// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
#define CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/bluetooth/web_bluetooth_pairing_manager_delegate.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace url {
class Origin;
}  // namespace url

namespace device {
class BluetoothDiscoverySession;
class BluetoothGattConnection;
class BluetoothGattNotifySession;
class BluetoothRemoteGattCharacteristic;
class BluetoothUUID;
}  // namespace device

namespace content {

class BluetoothAllowedDevices;
class BluetoothDeviceChooserController;
class BluetoothDeviceScanningPromptController;
struct CacheQueryResult;
class FrameConnectedBluetoothDevices;
struct GATTNotifySessionAndCharacteristicClient;
class RenderProcessHost;
class WebBluetoothPairingManager;

bool HasValidFilter(
    const std::optional<std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>&
        filters);

// Implementation of Mojo WebBluetoothService located in
// third_party/blink/renderer/modules/bluetooth.
// It handles Web Bluetooth API requests coming from Blink / renderer
// process and uses the platform abstraction of device/bluetooth.
// WebBluetoothServiceImpl is not thread-safe and should be created on the
// UI thread as required by device/bluetooth.
// This class is instantiated on-demand via Mojo's ConnectToRemoteService
// from the renderer when the first Web Bluetooth API request is handled.
class CONTENT_EXPORT WebBluetoothServiceImpl
    : public blink::mojom::WebBluetoothService,
      public DocumentUserData<WebBluetoothServiceImpl>,
      public WebContentsObserver,
      public device::BluetoothAdapter::Observer,
      public BluetoothDelegate::FramePermissionObserver,
      public WebBluetoothPairingManagerDelegate {
 public:
  static blink::mojom::WebBluetoothResult TranslateConnectErrorAndRecord(
      device::BluetoothDevice::ConnectErrorCode error_code);

  // Binds `receiver` to `WebBluetoothServiceImpl` for the currently active
  // document for `render_frame_host`, if no security checks fail. See
  // `DocumentUserData` for additional details about lifetime.
  static void BindIfAllowed(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);

  // Wrapper around `BindIfAllowed()` that also returns the created
  // WebBluetoothServiceImpl, if any.
  static WebBluetoothServiceImpl* CreateForTesting(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);

  // Calling this methods prevents WebBluetoothServiceImpl from clearing up its
  // WatchAdvertisement clients when the window either loses focus or becomes
  // hidden or occluded. This method is meant to be called from browser tests to
  // prevent flakiness.
  static void IgnoreVisibilityRequirementsForTesting();

  ~WebBluetoothServiceImpl() override;

  WebBluetoothServiceImpl(const WebBluetoothServiceImpl&) = delete;
  WebBluetoothServiceImpl& operator=(const WebBluetoothServiceImpl&) = delete;

  // Prefer `receiver_.ReportBadMessage()` in new code. Existing callers should
  // be migrated as well.
  void ReceivedBadMessage(bad_message::BadMessageReason reason);

  // Checks the current requesting and embedding origins as well as the policy
  // or global Web Bluetooth block to determine if Web Bluetooth is allowed.
  // Returns |SUCCESS| if Bluetooth is allowed.
  blink::mojom::WebBluetoothResult GetBluetoothAllowed();

  // Returns whether the device is paired with the |render_frame_host_|'s
  // GetLastCommittedOrigin().
  bool IsDevicePaired(const std::string& device_address);

  // Informs each client in |scanning_clients_| of the user's permission
  // decision.
  void OnBluetoothScanningPromptEvent(
      BluetoothScanningPrompt::Event event,
      BluetoothDeviceScanningPromptController* prompt_controller);

  // BluetoothDelegate::FramePermissionObserverimplementation:
  void OnPermissionRevoked(const url::Origin& origin) override;
  content::RenderFrameHost* GetRenderFrameHost() override;

#if PAIR_BLUETOOTH_ON_DEMAND()
  void SetPairingManagerForTesting(
      std::unique_ptr<WebBluetoothPairingManager> pairing_manager);
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

 private:
  friend DocumentUserData;

  DOCUMENT_USER_DATA_KEY_DECL();

  // `render_frame_host`: The RFH of the document that owns this instance.
  explicit WebBluetoothServiceImpl(RenderFrameHost* render_frame_host);

  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           DestroyedDuringRequestDevice);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           DestroyedDuringRequestScanningStart);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest, PermissionAllowed);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           PermissionPromptCanceled);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenTabHidden);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenTabOccluded);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenBlocked);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           BluetoothScanningPermissionRevokedWhenFocusIsLost);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ReadCharacteristicValueErrorWithValueIgnored);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplBrowserTest,
                           NoShowBluetoothScanningPromptInPrerendering);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           DeferredStartNotifySession);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest, DeviceDisconnected);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           DeviceGattServicesDiscoveryTimeout);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           TwoWatchAdvertisementsReqSuccess);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           TwoWatchAdvertisementsReqFail);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           SecWatchAdvertisementsReqAfterFirstSuccess);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTestWithBaseAdapter,
                           EmulatedAdapterRemovalRestoresOriginalAdapter);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ServiceDestroyedDuringAdapterAcquisition);

#if PAIR_BLUETOOTH_ON_DEMAND()
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           ReadCharacteristicValueNotAuthorized);
  FRIEND_TEST_ALL_PREFIXES(WebBluetoothServiceImplTest,
                           IncompletePairingOnShutdown);
#endif  // PAIR_BLUETOOTH_ON_DEMAND()

  friend class FrameConnectedBluetoothDevicesTest;
  friend class WebBluetoothServiceImplTest;
  friend class WebBluetoothServiceImplTestWithBaseAdapter;

  using PrimaryServicesRequestCallback =
      base::OnceCallback<void(device::BluetoothDevice*)>;
  using ScanFilters = std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>;

  class AdvertisementClient;
  class WatchAdvertisementsClient;
  class ScanningClient;
  struct DeferredStartNotificationData;

  // Returns false if `this` is already bound.
  bool Bind(mojo::PendingReceiver<blink::mojom::WebBluetoothService> receiver);

  // WebContentsObserver:
  // These functions should always check that the affected RenderFrameHost
  // is this->render_frame_host() and not some other frame in the same tab.
  void OnVisibilityChanged(Visibility visibility) override;
  void OnWebContentsLostFocus(RenderWidgetHost* render_widget_host) override;

  // BluetoothAdapter::Observer:
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceAdvertisementReceived(
      const std::string& device_address,
      const std::optional<std::string>& device_name,
      const std::optional<std::string>& advertisement_name,
      std::optional<int8_t> rssi,
      std::optional<int8_t> tx_power,
      std::optional<uint16_t> appearance,
      const device::BluetoothDevice::UUIDList& advertised_uuids,
      const device::BluetoothDevice::ServiceDataMap& service_data_map,
      const device::BluetoothDevice::ManufacturerDataMap& manufacturer_data_map)
      override;
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;
  void GattCharacteristicValueChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;

  // Notifies the WebBluetoothServiceClient that characteristic
  // |characteristic_instance_id| changed it's value. We only do this for
  // characteristics that have been returned to the client in the past.
  void NotifyCharacteristicValueChanged(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value);

  // WebBluetoothService methods:
  void GetAvailability(GetAvailabilityCallback callback) override;
  void RequestDevice(blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
                     RequestDeviceCallback callback) override;
  void GetDevices(GetDevicesCallback callback) override;
  void ForgetDevice(const blink::WebBluetoothDeviceId& device_id,
                    ForgetDeviceCallback callback) override;
  void RemoteServerConnect(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<blink::mojom::WebBluetoothServerClient>
          client,
      RemoteServerConnectCallback callback) override;
  void RemoteServerDisconnect(
      const blink::WebBluetoothDeviceId& device_id) override;
  void RemoteServerGetPrimaryServices(
      const blink::WebBluetoothDeviceId& device_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const std::optional<device::BluetoothUUID>& services_uuid,
      RemoteServerGetPrimaryServicesCallback callback) override;
  void RemoteServiceGetCharacteristics(
      const std::string& service_instance_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const std::optional<device::BluetoothUUID>& characteristics_uuid,
      RemoteServiceGetCharacteristicsCallback callback) override;
  void RemoteCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicReadValueCallback callback) override;
  void RemoteCharacteristicWriteValue(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      RemoteCharacteristicWriteValueCallback callback) override;
  void RemoteCharacteristicStartNotifications(
      const std::string& characteristic_instance_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothCharacteristicClient> client,
      RemoteCharacteristicStartNotificationsCallback callback) override;
  void RemoteCharacteristicStartNotificationsInternal(
      const std::string& characteristic_instance_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothCharacteristicClient>
          client,
      RemoteCharacteristicStartNotificationsCallback callback) override;
  void RemoteCharacteristicStopNotifications(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicStopNotificationsCallback callback) override;
  void RemoteCharacteristicGetDescriptors(
      const std::string& service_instance_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const std::optional<device::BluetoothUUID>& characteristics_uuid,
      RemoteCharacteristicGetDescriptorsCallback callback) override;
  void RemoteDescriptorReadValue(
      const std::string& descriptor_instance_id,
      RemoteDescriptorReadValueCallback callback) override;
  void RemoteDescriptorWriteValue(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      RemoteDescriptorWriteValueCallback callback) override;
  void RequestScanningStart(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_remote,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      RequestScanningStartCallback callback) override;
  void WatchAdvertisementsForDevice(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_remote,
      WatchAdvertisementsForDeviceCallback callback) override;

  void RequestDeviceImpl(
      blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
      RequestDeviceCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);

  void GetDevicesImpl(GetDevicesCallback callback,
                      scoped_refptr<device::BluetoothAdapter> adapter);

  // Callbacks for BLE scanning.
  void RequestScanningStartImpl(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_remote,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      RequestScanningStartCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartDiscoverySessionForScanning(
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_remote,
      blink::mojom::WebBluetoothRequestLEScanOptionsPtr options,
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoverySessionErrorForScanning();

  // Callbacks for watch advertisements for device.
  void WatchAdvertisementsForDeviceImpl(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::PendingAssociatedRemote<
          blink::mojom::WebBluetoothAdvertisementClient> client_remote,
      WatchAdvertisementsForDeviceCallback callback,
      scoped_refptr<device::BluetoothAdapter> adapter);
  void OnStartDiscoverySessionForWatchAdvertisements(
      std::unique_ptr<device::BluetoothDiscoverySession> session);
  void OnDiscoverySessionErrorForWatchAdvertisements();

  // Remove WatchAdvertisementsClients and ScanningClients with disconnected
  // WebBluetoothAdvertisementClients from their respective containers.
  void RemoveDisconnectedClients();

  // Stop active discovery sessions and destroy them if there aren't any active
  // AdvertisementClients.
  void MaybeStopDiscovery();

  // Should only be run after the services have been discovered for
  // |device_address|.
  void RemoteServerGetPrimaryServicesImpl(
      const blink::WebBluetoothDeviceId& device_id,
      blink::mojom::WebBluetoothGATTQueryQuantity quantity,
      const std::optional<device::BluetoothUUID>& services_uuid,
      RemoteServerGetPrimaryServicesCallback callback,
      device::BluetoothDevice* device);

  // Callback for BluetoothDeviceChooserController::GetDevice.
  void OnGetDevice(RequestDeviceCallback callback,
                   blink::mojom::WebBluetoothResult result,
                   blink::mojom::WebBluetoothRequestDeviceOptionsPtr options,
                   const std::string& device_id);

  // Callbacks for BluetoothDevice::CreateGattConnection.
  void OnCreateGATTConnection(
      const blink::WebBluetoothDeviceId& device_id,
      mojo::AssociatedRemote<blink::mojom::WebBluetoothServerClient> client,
      RemoteServerConnectCallback callback,
      std::unique_ptr<device::BluetoothGattConnection> connection,
      std::optional<device::BluetoothDevice::ConnectErrorCode> error_code);

  // Callbacks for BluetoothRemoteGattCharacteristic::ReadRemoteCharacteristic.
  void OnCharacteristicReadValue(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicReadValueCallback callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Callbacks for BluetoothRemoteGattCharacteristic::WriteRemoteCharacteristic.
  void OnCharacteristicWriteValueSuccess(
      RemoteCharacteristicWriteValueCallback callback);
  void OnCharacteristicWriteValueFailed(
      const std::string& characteristic_instance_id,
      const std::vector<uint8_t>& value,
      blink::mojom::WebBluetoothWriteType write_type,
      RemoteCharacteristicWriteValueCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Callbacks for BluetoothRemoteGattCharacteristic::StartNotifySession.
  void OnStartNotifySessionSuccess(
      RemoteCharacteristicStartNotificationsCallback callback,
      std::unique_ptr<device::BluetoothGattNotifySession> notify_session);
  void OnStartNotifySessionFailed(
      RemoteCharacteristicStartNotificationsCallback callback,
      const std::string& characteristic_instance_id,
      device::BluetoothGattService::GattErrorCode error_code);

  // Callback for BluetoothGattNotifySession::Stop.
  void OnStopNotifySessionComplete(
      const std::string& characteristic_instance_id,
      RemoteCharacteristicStopNotificationsCallback callback);

  // Callbacks for BluetoothRemoteGattDescriptor::ReadRemoteDescriptor.
  void OnDescriptorReadValue(
      const std::string& descriptor_instance_id,
      RemoteDescriptorReadValueCallback callback,
      std::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  // Callbacks for BluetoothRemoteGattDescriptor::WriteRemoteDescriptor.
  void OnDescriptorWriteValueSuccess(
      RemoteDescriptorWriteValueCallback callback);
  void OnDescriptorWriteValueFailed(
      const std::string& descriptor_instance_id,
      const std::vector<uint8_t>& value,
      RemoteDescriptorWriteValueCallback callback,
      device::BluetoothGattService::GattErrorCode error_code);

  // Functions to query the platform cache for the bluetooth object.
  // result.outcome == CacheQueryOutcome::SUCCESS if the object was found in the
  // cache. Otherwise result.outcome that can used to record the outcome and
  // result.error will contain the error that should be sent to the renderer.
  // One of the possible outcomes is BAD_RENDERER. In this case we crash the
  // renderer, record the reason and close the pipe, so it's safe to drop
  // any callbacks.

  // Queries the platform cache for a Device with |device_id| for |origin|.
  // Fills in the |outcome| field and the |device| field if successful.
  CacheQueryResult QueryCacheForDevice(
      const blink::WebBluetoothDeviceId& device_id);

  // Return the cached BluetoothDevice for the given |device_id|.
  device::BluetoothDevice* GetCachedDevice(
      const blink::WebBluetoothDeviceId& device_id);

  // Queries the platform cache for a Service with |service_instance_id|. Fills
  // in the |outcome| field, and |device| and |service| fields if successful.
  CacheQueryResult QueryCacheForService(const std::string& service_instance_id);

  // Queries the platform cache for a characteristic with
  // |characteristic_instance_id|. Fills in the |outcome| field, and |device|,
  // |service| and |characteristic| fields if successful.
  CacheQueryResult QueryCacheForCharacteristic(
      const std::string& characteristic_instance_id);

  // Queries the platform cache for a descriptor with |descriptor_instance_id|.
  // Fills in the |outcome| field, and |device|, |service|, |characteristic|,
  // |descriptor| fields if successful.
  CacheQueryResult QueryCacheForDescriptor(
      const std::string& descriptor_instance_id);

  void RunPendingPrimaryServicesRequests(device::BluetoothDevice* device);

  RenderProcessHost* GetRenderProcessHost();
  device::BluetoothAdapter* GetAdapter();
  BluetoothAllowedDevices& allowed_devices();

  void StoreAllowedScanOptions(
      const blink::mojom::WebBluetoothRequestLEScanOptions& options);
  bool AreScanFiltersAllowed(const std::optional<ScanFilters>& filters) const;

  // Clears state associated with Bluetooth LE Scanning.
  void ClearAdvertisementClients();

  bool IsAllowedToAccessAtLeastOneService(
      const blink::WebBluetoothDeviceId& device_id);
  bool IsAllowedToAccessService(const blink::WebBluetoothDeviceId& device_id,
                                const device::BluetoothUUID& service);
  bool IsAllowedToAccessManufacturerData(
      const blink::WebBluetoothDeviceId& device_id,
      uint16_t manufacturer_code);

  // Returns true if at least |ble_scan_discovery_session_| or
  // |watch_advertisements_discovery_session_| is active.
  bool HasActiveDiscoverySession();

  // WebBluetoothPairingManagerDelegate implementation:
  blink::WebBluetoothDeviceId GetCharacteristicDeviceID(
      const std::string& characteristic_instance_id) override;
  blink::WebBluetoothDeviceId GetDescriptorDeviceId(
      const std::string& descriptor_instance_id) override;
  blink::WebBluetoothDeviceId GetWebBluetoothDeviceId(
      const std::string& device_address) override;
  void PairDevice(const blink::WebBluetoothDeviceId& device_id,
                  device::BluetoothDevice::PairingDelegate* pairing_delegate,
                  device::BluetoothDevice::ConnectCallback callback) override;
  void CancelPairing(const blink::WebBluetoothDeviceId& device_id) override;
  void SetPinCode(const blink::WebBluetoothDeviceId& device_id,
                  const std::string& pincode) override;
  void PromptForBluetoothPairing(
      const std::u16string& device_identifier,
      BluetoothDelegate::PairPromptCallback callback,
      BluetoothDelegate::PairingKind pairing_kind,
      const std::optional<std::u16string>& pin) override;
  void PairConfirmed(const blink::WebBluetoothDeviceId& device_id) override;

  mojo::Receiver<blink::mojom::WebBluetoothService> receiver_;

  // Used to open a BluetoothChooser and start a device discovery session.
  std::unique_ptr<BluetoothDeviceChooserController> device_chooser_controller_;

  // Used to open a BluetoothScanningPrompt.
  std::unique_ptr<BluetoothDeviceScanningPromptController>
      device_scanning_prompt_controller_;

  // Maps to get the object's parent based on its instanceID.
  std::unordered_map<std::string, std::string> service_id_to_device_address_;
  std::unordered_map<std::string, std::string> characteristic_id_to_service_id_;
  std::unordered_map<std::string, std::string>
      descriptor_id_to_characteristic_id_;

  // Map to keep track of the connected Bluetooth devices.
  std::unique_ptr<FrameConnectedBluetoothDevices> connected_devices_;

  // Maps a device address to callbacks that are waiting for services to
  // be discovered for that device.
  std::unordered_map<std::string, std::vector<PrimaryServicesRequestCallback>>
      pending_primary_services_requests_;

  // Map to keep track of the characteristics' notify sessions.
  std::unordered_map<std::string,
                     std::unique_ptr<GATTNotifySessionAndCharacteristicClient>>
      characteristic_id_to_notify_session_;
  // Map characteristic instance ID to deferred startNotification data.
  std::unordered_map<
      std::string,
      base::queue<std::unique_ptr<DeferredStartNotificationData>>>
      characteristic_id_to_deferred_start_;

  // Keeps track of our BLE scanning session.
  std::unique_ptr<device::BluetoothDiscoverySession>
      ble_scan_discovery_session_;

  // Keeps track of our watch advertisements discovery session.
  std::unique_ptr<device::BluetoothDiscoverySession>
      watch_advertisements_discovery_session_;

  // This queues up a scanning start callback so that we only have one
  // BluetoothDiscoverySession start request at a time for a BLE scan.
  RequestScanningStartCallback request_scanning_start_callback_;

  // This queues up pending watch advertisements clients so that
  // we only have one BluetoothDiscoverySession start request at a time for
  // watching device advertisements.
  std::vector<std::unique_ptr<WatchAdvertisementsClient>>
      watch_advertisements_pending_clients_;

  // List of clients that we must broadcast scan changes to.
  std::vector<std::unique_ptr<ScanningClient>> scanning_clients_;
  std::vector<std::unique_ptr<WatchAdvertisementsClient>>
      watch_advertisements_clients_;

  // Allowed Bluetooth scanning filters.
  ScanFilters allowed_scan_filters_;

  // Whether a site has been allowed to receive all Bluetooth advertisement
  // packets.
  bool accept_all_advertisements_ = false;

#if PAIR_BLUETOOTH_ON_DEMAND()
  std::unique_ptr<WebBluetoothPairingManager> pairing_manager_;
#endif

  base::ScopedObservation<BluetoothDelegate,
                          BluetoothDelegate::FramePermissionObserver>
      observer_{this};

  base::WeakPtrFactory<WebBluetoothServiceImpl> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_WEB_BLUETOOTH_SERVICE_IMPL_H_
