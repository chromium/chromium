// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_BLUETOOTH_FLOSS_FLOSS_SOCKET_MANAGER_H_
#define DEVICE_BLUETOOTH_FLOSS_FLOSS_SOCKET_MANAGER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/exported_object.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/floss/floss_dbus_client.h"
#include "device/bluetooth/public/cpp/bluetooth_uuid.h"

namespace floss {

// The socket manager allows creation and connection of RFCOMM/L2CAP services.
// It is managed by FlossClientBundle and will be initialized with an adapter
// when one is powered on.
class DEVICE_BLUETOOTH_EXPORT FlossSocketManager : public FlossDBusClient {
 public:
  // Id given after register callbacks.
  using CallbackId = uint32_t;

  // Id given after creating any socket type.
  using SocketId = uint64_t;

  // Supported socket types for api.
  enum class SocketType {
    kUnknown = 0,
    kRfcomm = 1,
    // Not supported via the socket manager api.
    kSco_DONOTUSE = 2,
    kL2cap = 3,
    kL2capLe = 4,
  };

  // States for server socket.
  enum class ServerSocketState {
    // FlossListeningSocket is ready. Call |Accept| to accept incoming
    // connections.
    kReady,

    // FlossListeningSocket is closed.
    kClosed,
  };

  // Security level of connection.
  enum class Security {
    kInsecure,
    kSecure,
  };

  // Flags for changing how Floss constructs a socket.
  enum class SocketFlags : int {
    kSocketFlagsEncrypt = 1 << 0,
    kSocketFlagsAuth = 1 << 1,
    kSocketFlagsNoSdp = 1 << 2,
    kSocketFlagsAuthMitm = 1 << 3,
    kSocketFlagsAuth16Digit = 1 << 4
  };

  static int GetRawFlossFlagsFromBluetoothFlags(bool encrypt,
                                                bool auth,
                                                bool auth_mitm,
                                                bool auth_16_digit,
                                                bool no_sdp);

  // Represents a listening socket.
  struct FlossListeningSocket {
    SocketId id = FlossSocketManager::kInvalidSocketId;
    SocketType type = SocketType::kUnknown;
    int flags = 0;
    std::optional<int> psm;
    std::optional<int> channel;
    std::optional<std::string> name;
    std::optional<device::BluetoothUUID> uuid;

    FlossListeningSocket();
    FlossListeningSocket(const FlossListeningSocket&);
    ~FlossListeningSocket();

    bool is_valid() const { return id != FlossSocketManager::kInvalidSocketId; }
  };

  // Represents a connecting socket (either incoming or outgoing).
  struct FlossSocket {
    SocketId id = FlossSocketManager::kInvalidSocketId;
    FlossDeviceId remote_device;
    SocketType type = SocketType::kUnknown;
    int flags = 0;
    std::optional<base::ScopedFD> fd;
    int port = 0;
    std::optional<device::BluetoothUUID> uuid;
    int max_rx_size = 0;
    int max_tx_size = 0;

    FlossSocket();
    ~FlossSocket();

    // Due to ScopedFD, we don't want to use copy constructor for this.
    FlossSocket(const FlossSocket&) = delete;
    FlossSocket& operator=(const FlossSocket&) = delete;

    // Move constructor and assignment operator is ok.
    FlossSocket(FlossSocket&&);
    FlossSocket& operator=(FlossSocket&&) = default;

    bool is_valid() const { return id != FlossSocketManager::kInvalidSocketId; }
  };

  // Represents a result from any socket api call.
  struct SocketResult {
    BtifStatus status;
    SocketId id;
  };

  // Callback sent when a listening socket is ready to accept connections.
  using ConnectionStateChanged = base::RepeatingCallback<
      void(ServerSocketState, FlossListeningSocket, BtifStatus)>;

  // Callback used when a listening socket accepts new connections.
  using ConnectionAccepted = base::RepeatingCallback<void(FlossSocket&&)>;

  // Callback when a connection socket completes.
  using ConnectionCompleted =
      base::OnceCallback<void(BtifStatus, std::optional<FlossSocket>&&)>;

  // Error: Callback id is invalid.
  static const char kErrorInvalidCallback[];

  // Valid callback ids are always greater than 0.
  static const CallbackId kInvalidCallbackId = 0;

  // Valid socket ids are always greater than 0.
  static const SocketId kInvalidSocketId = 0;

  static std::unique_ptr<FlossSocketManager> Create();

  FlossSocketManager(const FlossSocketManager&) = delete;
  FlossSocketManager& operator=(const FlossSocketManager&) = delete;

  FlossSocketManager();
  ~FlossSocketManager() override;

  // Listen for connections using a L2Cap channel.
  virtual void ListenUsingL2cap(const Security security_level,
                                ResponseCallback<BtifStatus> callback,
                                ConnectionStateChanged ready_cb,
                                ConnectionAccepted new_connection_cb);

  // Listen for connections using a connection oriented LE L2Cap channel.
  virtual void ListenUsingL2capLe(const Security security_level,
                                  ResponseCallback<BtifStatus> callback,
                                  ConnectionStateChanged ready_cb,
                                  ConnectionAccepted new_connection_cb);

  // Listen for connections using an RFCOMM channel. This API exposes all of the
  // options supported by Floss and should only be used if there are no safer
  // variants capable of supporting a use-case, such as when manually
  // constructing SDP records for a listening socket.
  virtual void ListenUsingRfcommAlt(
      const std::optional<std::string> name,
      const std::optional<device::BluetoothUUID> application_uuid,
      const std::optional<int> channel,
      const std::optional<int> flags,
      ResponseCallback<BtifStatus> callback,
      ConnectionStateChanged ready_cb,
      ConnectionAccepted new_connection_cb);

  // Listen for connections using an RFCOMM channel. Creates SDP record with
  // given name and UUID.
  virtual void ListenUsingRfcomm(const std::string& name,
                                 const device::BluetoothUUID& uuid,
                                 const Security security_level,
                                 ResponseCallback<BtifStatus> callback,
                                 ConnectionStateChanged ready_cb,
                                 ConnectionAccepted new_connection_cb);

  // Connect via a L2Cap channel on given psm.
  virtual void ConnectUsingL2cap(const FlossDeviceId& remote_device,
                                 const int psm,
                                 const Security security_level,
                                 ConnectionCompleted callback);

  // Connect via a connection oriented LE L2Cap channel on given psm.
  virtual void ConnectUsingL2capLe(const FlossDeviceId& remote_device,
                                   const int psm,
                                   const Security security_level,
                                   ConnectionCompleted callback);

  // Connect to a remote service using a RFCOMM channel.
  virtual void ConnectUsingRfcomm(const FlossDeviceId& remote_device,
                                  const device::BluetoothUUID& uuid,
                                  const Security security_level,
                                  ConnectionCompleted callback);

  // Accept new connections on |id|. If the given SocketId is not a listening
  // socket or closed, the callback will receive a failing |BtifStatus| value.
  virtual void Accept(const SocketId id,
                      std::optional<uint32_t> timeout_ms,
                      ResponseCallback<BtifStatus> callback);

  // Closes the socket on |id|. Only works for listening sockets. For connecting
  // sockets, simply close the fd to terminate the connection.
  virtual void Close(const SocketId id, ResponseCallback<BtifStatus> callback);

  // Initializes the socket manager with given adapter.
  void Init(dbus::Bus* bus,
            const std::string& service_name,
            const int adapter_index,
            base::Version version,
            base::OnceClosure on_ready) override;

 protected:
  friend class FlossSocketManagerTest;

  // Complete the method call for |RegisterCallback|.
  void CompleteRegisterCallback(dbus::Response* response,
                                dbus::ErrorResponse* error_response);

  // Complete the method call for |UnregisterCallback|.
  void CompleteUnregisterCallback(DBusResult<bool> result);

  // Complete any of |ListenUsingL2cap| or |ListenUsingRfcomm|.
  void CompleteListen(ResponseCallback<BtifStatus> callback,
                      ConnectionStateChanged ready_cb,
                      ConnectionAccepted new_connection_cb,
                      DBusResult<SocketResult> result);

  // Complete any of |ConnectUsingL2cap| or |ConnectUsingRfcomm|.
  void CompleteConnect(ConnectionCompleted callback,
                       DBusResult<SocketResult> result);

  // Handle callback |IncomingSocketReady| on exported object path.
  void OnIncomingSocketReady(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |IncomingSocketClosed| on exported object path.
  void OnIncomingSocketClosed(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |HandleIncomingConnection| on exported object path.
  void OnHandleIncomingConnection(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Handle callback |OutgoingConnectionResult| on exported object path.
  void OnOutgoingConnectionResult(
      dbus::MethodCall* method_call,
      dbus::ExportedObject::ResponseSender response_sender);

  // Managed by FlossDBusManager - we keep local pointer to access object proxy.
  raw_ptr<dbus::Bus> bus_ = nullptr;

  // Adapter used for socket connections by this class.
  dbus::ObjectPath adapter_path_;

  // Service which implements the SocketManager interface.
  std::string service_name_;

  // Map of listening sockets to callbacks.
  std::unordered_map<SocketId,
                     std::pair<ConnectionStateChanged, ConnectionAccepted>>
      listening_sockets_to_callbacks_;

  // Map of connection sockets that haven't completed.
  std::unordered_map<SocketId, ConnectionCompleted>
      connecting_sockets_to_callbacks_;

 private:
  template <typename R, typename... Args>
  void CallSocketMethod(ResponseCallback<R> callback,
                        const char* member,
                        Args... args) {
    CallMethod(std::move(callback), bus_, service_name_,
               kSocketManagerInterface, adapter_path_, member, args...);
  }

  // Object path for exported callbacks registered against manager interface.
  static const char kExportedCallbacksPath[];

  // All socket api calls require callback id since callbacks must take
  // ownership of the file descriptors. A value of zero is invalid.
  CallbackId callback_id_ = kInvalidCallbackId;

  // Signal when client is ready to be used.
  base::OnceClosure on_ready_;

  base::WeakPtrFactory<FlossSocketManager> weak_ptr_factory_{this};
};
}  // namespace floss

#endif  // DEVICE_BLUETOOTH_FLOSS_FLOSS_SOCKET_MANAGER_H_
