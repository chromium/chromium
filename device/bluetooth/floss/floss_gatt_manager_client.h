// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_MANAGER_CLIENT_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_MANAGER_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/exported_callback_manager.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

// Authentication requirements for GATT.
enum class DEVICE_BLUETOOTH_EXPORT AuthRequired {
  kNoAuth = 0,  // No authentication required.
  kNoMitm,      // Encrypted but not authenticated.
  kReqMitm,     // Encrypted and authenticated.

  // Same as above but signed + encrypted.
  kSignedNoMitm,
  kSignedReqMitm,
};

enum class DEVICE_BLUETOOTH_EXPORT WriteType {
  kInvalid = 0,
  kWriteNoResponse,
  kWrite,
  kWritePrepare,
};

enum class DEVICE_BLUETOOTH_EXPORT LeDiscoverableMode {
  kInvalid = 0,
  kNonDiscoverable = 1,
  kLimitedDiscoverable = 2,
  kGeneralDiscoverable = 3,
};

enum class DEVICE_BLUETOOTH_EXPORT LePhy {
  kInvalid = 0,
  kPhy1m = 1,
  kPhy2m = 2,
  kPhyCoded = 3,
};

// Status for many GATT apis. Due to complexity here, only kSuccess should be
// used for comparisons.
enum class DEVICE_BLUETOOTH_EXPORT GattStatus {
  kSuccess = 0,
  kInvalidHandle,
  kReadNotPermitted,
  kWriteNotPermitted,
  kInvalidPdu,
  kInsufficientAuthentication,
  kReqNotSupported,
  kInvalidOffset,
  kInsufficientAuthorization,
  kPrepareQueueFull,
  kNotFound,
  kNotLong,
  kInsufficientKeySize,
  kInvalidAttributeLen,
  kUnlikelyError,
  kInsufficientEncryption,
  kUnsupportedGroupType,
  kInsufficientResources,
  kDatabaseOutOfSync,
  kValueNotAllowed,
  // Big jump here
  kTooShort = 0x7f,
  kNoResources,
  kInternalError,
  kWrongState,
  kDbFull,
  kBusy,
  kError,
  kCommandStarted,
  kIllegalParameter,
  kPending,
  kAuthFailed,
  kMore,
  kInvalidConfig,
  kServiceStarted,
  kEncryptedNoMitm,
  kNotEncrypted,
  kCongested,
  kDupReg,
  kAlreadyOpen,
  kCancel,
  // 0xE0 - 0xFC reserved for future use.
  kCccCfgErr = 0xFD,
  kPrcInProgress = 0xFE,
  kOutOfRange = 0xFF,
};

// GATT WriteCharacteristic D-Bus method results.
enum class DEVICE_BLUETOOTH_EXPORT GattWriteRequestStatus {
  kSuccess = 0,
  kFail = 1,
  kBusy = 2,
};

struct DEVICE_BLUETOOTH_EXPORT GattDescriptor {
  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t permissions;

  GattDescriptor();
  ~GattDescriptor();
};

struct DEVICE_BLUETOOTH_EXPORT GattCharacteristic {
  enum Property {
    GATT_CHAR_PROP_BIT_BROADCAST = (1 << 0),
    GATT_CHAR_PROP_BIT_READ = (1 << 1),
    GATT_CHAR_PROP_BIT_WRITE_NR = (1 << 2),
    GATT_CHAR_PROP_BIT_WRITE = (1 << 3),
    GATT_CHAR_PROP_BIT_NOTIFY = (1 << 4),
    GATT_CHAR_PROP_BIT_INDICATE = (1 << 5),
    GATT_CHAR_PROP_BIT_AUTH = (1 << 6),
    GATT_CHAR_PROP_BIT_EXT_PROP = (1 << 7),
  };

  enum Permission {
    GATT_PERM_READ = (1 << 0),
    GATT_PERM_READ_ENCRYPTED = (1 << 1),
    GATT_PERM_READ_ENC_MITM = (1 << 2),
    GATT_PERM_WRITE = (1 << 4),
    GATT_PERM_WRITE_ENCRYPTED = (1 << 5),
    GATT_PERM_WRITE_ENC_MITM = (1 << 6),
    GATT_PERM_WRITE_SIGNED = (1 << 7),
    GATT_PERM_WRITE_SIGNED_MITM = (1 << 8),
  };

  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t properties;
  int32_t permissions;
  int32_t key_size;
  WriteType write_type;
  std::vector<GattDescriptor> descriptors;

  GattCharacteristic();
  GattCharacteristic(const GattCharacteristic&);
  ~GattCharacteristic();
};

struct DEVICE_BLUETOOTH_EXPORT GattService {
  enum ServiceType {
    GATT_SERVICE_TYPE_PRIMARY = 0,
    GATT_SERVICE_TYPE_SECONDARY = 1,
  };
  device::BluetoothUUID uuid;
  int32_t instance_id;
  int32_t service_type;
  std::vector<GattCharacteristic> characteristics;
  std::vector<GattService> included_services;

  GattService();
  GattService(const GattService&);
  ~GattService();
};

// Callback functions expected to be imported by the GATT client.
//
// This also doubles as an observer class for the GATT client since it will
// really only filter out calls that aren't for this client.
class DEVICE_BLUETOOTH_EXPORT FlossGattClientObserver
    : public base::CheckedObserver {
 public:
  FlossGattClientObserver(const FlossGattClientObserver&) = delete;
  FlossGattClientObserver& operator=(const FlossGattClientObserver&) = delete;

  FlossGattClientObserver() = default;
  ~FlossGattClientObserver() override = default;

  // A client has completed registration for callbacks. Subsequent uses should
  // use this client id.
  virtual void GattClientRegistered(GattStatus status, int32_t client_id) {}

  // A client connection has changed state.
  virtual void GattClientConnectionState(GattStatus status,
                                         int32_t client_id,
                                         bool connected,
                                         std::string address) {}

  // The PHY used for a connection has changed states.
  virtual void GattPhyUpdate(std::string address,
                             LePhy tx,
                             LePhy rx,
                             GattStatus status) {}

  // Result of reading the currently used PHY.
  virtual void GattPhyRead(std::string address,
                           LePhy tx,
                           LePhy rx,
                           GattStatus status) {}

  // Service resolution completed and GATT db available.
  virtual void GattSearchComplete(std::string address,
                                  const std::vector<GattService>& services,
                                  GattStatus status) {}

  // Result of reading a characteristic.
  virtual void GattCharacteristicRead(std::string address,
                                      GattStatus status,
                                      int32_t handle,
                                      const std::vector<uint8_t>& data) {}

  // Result of writing a characteristic.
  virtual void GattCharacteristicWrite(std::string address,
                                       GattStatus status,
                                       int32_t handle) {}

  // Reliable write completed.
  virtual void GattExecuteWrite(std::string address, GattStatus status) {}

  // Result of reading a descriptor.
  virtual void GattDescriptorRead(std::string address,
                                  GattStatus status,
                                  int32_t handle,
                                  const std::vector<uint8_t>& data) {}

  // Result of writing to a descriptor.
  virtual void GattDescriptorWrite(std::string address,
                                   GattStatus status,
                                   int32_t handle) {}

  // Notification or indication of a handle on a remote device.
  virtual void GattNotify(std::string address,
                          int32_t handle,
                          const std::vector<uint8_t>& data) {}

  // Result of reading remote rssi.
  virtual void GattReadRemoteRssi(std::string address,
                                  int32_t rssi,
                                  GattStatus status) {}

  // Result of setting connection mtu.
  virtual void GattConfigureMtu(std::string address,
                                int32_t mtu,
                                GattStatus status) {}

  // Change to connection parameters.
  virtual void GattConnectionUpdated(std::string address,
                                     int32_t interval,
                                     int32_t latency,
                                     int32_t timeout,
                                     GattStatus status) {}

  // Notification from the peer that some records are updated, so a re-discovery
  // is in order.
  virtual void GattServiceChanged(std::string address) {}
};

// Callback functions expected to be imported by the GATT server.
//
// This also doubles as an observer class for the GATT server.
class DEVICE_BLUETOOTH_EXPORT FlossGattServerObserver
    : public base::CheckedObserver {
 public:
  FlossGattServerObserver(const FlossGattServerObserver&) = delete;
  FlossGattServerObserver& operator=(const FlossGattServerObserver&) = delete;

  FlossGattServerObserver() = default;
  ~FlossGattServerObserver() override = default;

  // Server has completed registration for callbacks and set a server id for
  // subsequent use.
  virtual void GattServerRegistered(GattStatus status, int32_t server) {}

  // A server connection has changed state.
  virtual void GattServerConnectionState(int32_t server_id,
                                         bool connected,
                                         std::string address) {}

  // A Gatt service has been added to the server.
  virtual void GattServerServiceAdded(GattStatus status, GattService service) {}

  // A Gatt service has been removed from the server.
  virtual void GattServerServiceRemoved(GattStatus status, int32_t handle) {}

  // A remote device has requested to read a Gatt server characteristic.
  virtual void GattServerCharacteristicReadRequest(std::string address,
                                                   int32_t request_id,
                                                   int32_t offset,
                                                   bool is_long,
                                                   int32_t handle) {}

  // A remote device has requested to read a Gatt server descriptor.
  virtual void GattServerDescriptorReadRequest(std::string address,
                                               int32_t request_id,
                                               int32_t offset,
                                               bool is_long,
                                               int32_t handle) {}

  // A remote device has requested to write to a Gatt server characteristic.
  virtual void GattServerCharacteristicWriteRequest(
      std::string address,
      int32_t request_id,
      int32_t offset,
      int32_t length,
      bool is_prepared_write,
      bool needs_response,
      int32_t handle,
      std::vector<uint8_t> value) {}

  // A remote device has requested to write to a Gatt server descriptor.
  virtual void GattServerDescriptorWriteRequest(std::string address,
                                                int32_t request_id,
                                                int32_t offset,
                                                int32_t length,
                                                bool is_prepared_write,
                                                bool needs_response,
                                                int32_t handle,
                                                std::vector<uint8_t> value) {}

  // Executes a write transaction for a given remote device.
  virtual void GattServerExecuteWrite(std::string address,
                                      int32_t request_id,
                                      bool execute_write) {}

  // A server sent out a notification.
  virtual void GattServerNotificationSent(std::string address,
                                          GattStatus status) {}

  // The MTU for a given device connection has changed.
  virtual void GattServerMtuChanged(std::string address, int32_t mtu) {}

  // A remote device changed the PHY.
  virtual void GattServerPhyUpdate(std::string address,
                                   LePhy tx_phy,
                                   LePhy rx_phy,
                                   GattStatus status) {}

  // A remote device read the PHY.
  virtual void GattServerPhyRead(std::string address,
                                 LePhy tx_phy,
                                 LePhy rx_phy,
                                 GattStatus status) {}

  // A given connection has been updated.
  virtual void GattServerConnectionUpdate(std::string address,
                                          int32_t interval,
                                          int32_t latency,
                                          int32_t timeout,
                                          GattStatus status) {}

  // Subrate connection parameters have been updated.
  virtual void GattServerSubrateChange(std::string address,
                                       int32_t subrate_factor,
                                       int32_t latency,
                                       int32_t continuation_num,
                                       int32_t timeout,
                                       GattStatus status) {}
};

class DEVICE_BLUETOOTH_EXPORT FlossGattManagerClient
    : public FlossDBusClient,
      public FlossGattClientObserver,
      public FlossGattServerObserver {
 public:
  static const char kExportedCallbacksPath[];

  // Creates the instance.
  static std::unique_ptr<FlossGattManagerClient> Create();

  FlossGattManagerClient();
  ~FlossGattManagerClient() override;

  FlossGattManagerClient(const FlossGattManagerClient&) = delete;
  FlossGattManagerClient& operator=(const FlossGattManagerClient&) = delete;

  // Manage observers.
  void AddObserver(FlossGattClientObserver* observer);
  void AddServerObserver(FlossGattServerObserver* observer);
  void RemoveObserver(FlossGattClientObserver* observer);
  void RemoveServerObserver(FlossGattServerObserver* observer);

  // Create a GATT client connection to a remote device on given transport.
  virtual void Connect(ResponseCallback<Void> callback,
                       const std::string& remote_device,
                       const BluetoothTransport& transport,
                       bool is_direct);

  // Disconnect GATT for given device.
  virtual void Disconnect(ResponseCallback<Void> callback,
                          const std::string& remote_device);

  // Start a reliable write for the remote device.
  virtual void BeginReliableWrite(ResponseCallback<Void> callback,
                                  const std::string& remote_device);

  // Execute or abort (depending on the value of |execute|) a reliable write for
  // the remote device.
  virtual void EndReliableWrite(ResponseCallback<Void> callback,
                                const std::string& remote_device,
                                bool execute);

  // Clears the attribute cache of a device.
  virtual void Refresh(ResponseCallback<Void> callback,
                       const std::string& remote_device);

  // Enumerates all GATT services on an already connected device.
  virtual void DiscoverAllServices(ResponseCallback<Void> callback,
                                   const std::string& remote_device);

  // Search for a GATT service on a connected device with a UUID.
  virtual void DiscoverServiceByUuid(ResponseCallback<Void> callback,
                                     const std::string& remote_device,
                                     const device::BluetoothUUID& uuid);

  // Reads a characteristic on a connected device with given |handle|.
  virtual void ReadCharacteristic(ResponseCallback<Void> callback,
                                  const std::string& remote_device,
                                  const int32_t handle,
                                  const AuthRequired auth_required);

  // Reads a characteristic on a connected device between |start_handle| and
  // |end_handle| that matches the given |uuid|.
  virtual void ReadUsingCharacteristicUuid(ResponseCallback<Void> callback,
                                           const std::string& remote_device,
                                           const device::BluetoothUUID& uuid,
                                           const int32_t start_handle,
                                           const int32_t end_handle,
                                           const AuthRequired auth_required);

  // Writes a characteristic on a connected device with given |handle|.
  virtual void WriteCharacteristic(
      ResponseCallback<GattWriteRequestStatus> callback,
      const std::string& remote_device,
      const int32_t handle,
      const WriteType write_type,
      const AuthRequired auth_required,
      const std::vector<uint8_t> data);

  // Reads the descriptor for a given characteristic |handle|.
  virtual void ReadDescriptor(ResponseCallback<Void> callback,
                              const std::string& remote_device,
                              const int32_t handle,
                              const AuthRequired auth_required);

  // Writes a descriptor for a given characteristic |handle|.
  virtual void WriteDescriptor(ResponseCallback<Void> callback,
                               const std::string& remote_device,
                               const int32_t handle,
                               const AuthRequired auth_required,
                               const std::vector<uint8_t> data);

  // Register for updates on a specific characteristic.
  virtual void RegisterForNotification(ResponseCallback<GattStatus> callback,
                                       const std::string& remote_device,
                                       const int32_t handle);

  // Unregister for updates on a specific characteristic.
  virtual void UnregisterNotification(ResponseCallback<GattStatus> callback,
                                      const std::string& remote_device,
                                      const int32_t handle);

  // Request RSSI for the connected device.
  virtual void ReadRemoteRssi(ResponseCallback<Void> callback,
                              const std::string& remote_device);

  // Configures the MTU for a connected device.
  virtual void ConfigureMTU(ResponseCallback<Void> callback,
                            const std::string& remote_device,
                            const int32_t mtu);

  // Update the connection parameters for the given device.
  virtual void UpdateConnectionParameters(ResponseCallback<Void> callback,
                                          const std::string& remote_device,
                                          const int32_t min_interval,
                                          const int32_t max_interval,
                                          const int32_t latency,
                                          const int32_t timeout,
                                          const uint16_t min_ce_len,
                                          const uint16_t max_ce_len);

  // Create a GATT server connection to a remote device on given transport.
  virtual void ServerConnect(ResponseCallback<Void> callback,
                             const std::string& remote_device,
                             const BluetoothTransport& transport);

  // Disconnect GATT for given device.
  virtual void ServerDisconnect(ResponseCallback<Void> callback,
                                const std::string& remote_device);

  // Set the preferred connection PHY.
  virtual void ServerSetPreferredPhy(ResponseCallback<Void> callback,
                                     const std::string& remote_device,
                                     LePhy tx_phy,
                                     LePhy rx_phy,
                                     int32_t phy_options);

  // Read the current transmitter and receiver PHY of the connection.
  virtual void ServerReadPhy(ResponseCallback<Void> callback,
                             const std::string& remote_device);

  // Add a Gatt service.
  virtual void AddService(ResponseCallback<Void> callback, GattService service);

  // Remove Gatt service.
  virtual void RemoveService(ResponseCallback<Void> callback, int32_t handle);

  // Clear all Gatt services.
  virtual void ClearServices(ResponseCallback<Void> callback);

  // Send a response to a given device.
  virtual void SendResponse(ResponseCallback<Void> callback,
                            const std::string& remote_device,
                            int32_t request_id,
                            GattStatus status,
                            int32_t offset,
                            std::vector<uint8_t> value);

  // Send a notification, with the option to specify whether or not the client
  // must confirm explicit acknowledgement.
  virtual void ServerSendNotification(ResponseCallback<Void> callback,
                                      const std::string& remote_device,
                                      int32_t handle,
                                      bool confirm,
                                      std::vector<uint8_t> value);
  // Get service name.
  std::string ServiceName() const { return service_name_; }
  // Get whether MSFT extension is supported.
  bool GetMsftSupported() const { return property_msft_supported_.Get(); }

  // Initialize the gatt client for the given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  friend class BluetoothGattFlossTest;

  // FlossGattClientObserver overrides
  void GattClientRegistered(GattStatus status, int32_t client_id) override;
  void GattClientConnectionState(GattStatus status,
                                 int32_t client_id,
                                 bool connected,
                                 std::string address) override;
  void GattPhyUpdate(std::string address,
                     LePhy tx,
                     LePhy rx,
                     GattStatus status) override;
  void GattPhyRead(std::string address,
                   LePhy tx,
                   LePhy rx,
                   GattStatus status) override;
  void GattSearchComplete(std::string address,
                          const std::vector<GattService>& services,
                          GattStatus status) override;
  void GattCharacteristicRead(std::string address,
                              GattStatus status,
                              int32_t handle,
                              const std::vector<uint8_t>& data) override;
  void GattCharacteristicWrite(std::string address,
                               GattStatus status,
                               int32_t handle) override;
  void GattExecuteWrite(std::string address, GattStatus status) override;
  void GattDescriptorRead(std::string address,
                          GattStatus status,
                          int32_t handle,
                          const std::vector<uint8_t>& data) override;
  void GattDescriptorWrite(std::string address,
                           GattStatus status,
                           int32_t handle) override;
  void GattNotify(std::string address,
                  int32_t handle,
                  const std::vector<uint8_t>& data) override;
  void GattReadRemoteRssi(std::string address,
                          int32_t rssi,
                          GattStatus status) override;
  void GattConfigureMtu(std::string address,
                        int32_t mtu,
                        GattStatus status) override;
  void GattConnectionUpdated(std::string address,
                             int32_t interval,
                             int32_t latency,
                             int32_t timeout,
                             GattStatus status) override;
  void GattServiceChanged(std::string address) override;
  void OnRegisterNotificationResponse(ResponseCallback<GattStatus> callback,
                                      bool is_registering,
                                      DBusResult<Void> result);

  // FlossGattServerObserver overrides
  void GattServerRegistered(GattStatus status, int32_t server_id) override;
  void GattServerConnectionState(int32_t server_id,
                                 bool connected,
                                 std::string address) override;
  void GattServerServiceAdded(GattStatus status, GattService service) override;
  void GattServerServiceRemoved(GattStatus status, int32_t handle) override;
  void GattServerCharacteristicReadRequest(std::string address,
                                           int32_t request_id,
                                           int32_t offset,
                                           bool is_long,
                                           int32_t handle) override;
  void GattServerDescriptorReadRequest(std::string address,
                                       int32_t request_id,
                                       int32_t offset,
                                       bool is_long,
                                       int32_t handle) override;
  void GattServerCharacteristicWriteRequest(
      std::string address,
      int32_t request_id,
      int32_t offset,
      int32_t length,
      bool is_prepared_write,
      bool needs_response,
      int32_t handle,
      std::vector<uint8_t> value) override;
  void GattServerDescriptorWriteRequest(std::string address,
                                        int32_t request_id,
                                        int32_t offset,
                                        int32_t length,
                                        bool is_prepared_write,
                                        bool needs_response,
                                        int32_t handle,
                                        std::vector<uint8_t> value) override;
  void GattServerExecuteWrite(std::string address,
                              int32_t request_id,
                              bool execute_write) override;
  void GattServerNotificationSent(std::string address,
                                  GattStatus status) override;
  void GattServerMtuChanged(std::string address, int32_t mtu) override;
  void GattServerPhyUpdate(std::string address,
                           LePhy tx_phy,
                           LePhy rx_phy,
                           GattStatus status) override;
  void GattServerPhyRead(std::string address,
                         LePhy tx_phy,
                         LePhy rx_phy,
                         GattStatus status) override;
  void GattServerConnectionUpdate(std::string address,
                                  int32_t interval,
                                  int32_t latency,
                                  int32_t timeout,
                                  GattStatus status) override;
  void GattServerSubrateChange(std::string address,
                               int32_t subrate_factor,
                               int32_t latency,
                               int32_t continuation_num,
                               int32_t timeout,
                               GattStatus status) override;

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Path used for gatt api calls by this class.
  dbus::ObjectPath gatt_adapter_path_;

  // List of observers interested in event notifications from this client.
  base::ObserverList<FlossGattClientObserver> gatt_client_observers_;
  base::ObserverList<FlossGattServerObserver> gatt_server_observers_;

  // Service which implements the GattManagerClient interface.
  std::string service_name_;

 private:
  friend class FlossGattClientTest;

  // Register gatt client and gatt server to get client and server ids.
  void RegisterClient();
  void RegisterServer();

  // Complete initialization and signal we're ready.
  void CompleteInit();

  template <typename R, typename... Args>
  void CallGattMethod(ResponseCallback<R> callback,
                      const char* member,
                      Args... args) {
    CallMethod(std::move(callback), bus_, service_name_, kGattInterface,
               gatt_adapter_path_, member, args...);
  }

  // Id given for registering as a gatt client and gatt server against Floss.
  // Used in many apis.
  int32_t client_id_ = 0;
  int32_t server_id_ = 0;

  // Signal when the client is ready to be used.
  base::OnceClosure on_ready_;

  // Exported callbacks for interacting with daemon.
  ExportedCallbackManager<FlossGattClientObserver>
      gatt_client_exported_callback_manager_{gatt::kCallbackInterface};
  ExportedCallbackManager<FlossGattServerObserver>
      gatt_server_exported_callback_manager_{gatt::kServerCallbackInterface};

  FlossProperty<bool> property_msft_supported_{
      kGattInterface, gatt::kCallbackInterface, "IsMsftSupported",
      nullptr /* property is not updateable */};

  base::WeakPtrFactory<FlossGattManagerClient> weak_ptr_factory_{this};
};

}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_GATT_MANAGER_CLIENT_H_
