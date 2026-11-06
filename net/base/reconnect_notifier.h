// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_RECONNECT_NOTIFIER_H_
#define NET_BASE_RECONNECT_NOTIFIER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "net/base/net_export.h"
#include "net/socket/next_proto.h"

namespace net {

// LINT.IfChange(ConnectionEstablishmentInitiator)
enum class ConnectionEstablishmentInitiator {
  // The session was established for a preconnect request.
  kPreconnect,
  // The session was established for a normal request.
  kRequest,
  kMaxValue = kRequest
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/navigation/enums.xml:ConnectionEstablishmentInitiator)

// TODO(crbug.com/406022435): Refactor the components to elsewhere to avoid
// exposing un-necessary data structures to the browser.

// An enum which represents the possible network change event that may happen
// in the underlying network connection.
enum class NetworkChangeEvent {
  // The current network is soon to be disconnected.
  kSoonToDisconnect,
  // Disconnected from the previously connected network.
  kDisconnected,
  // Connected to a new network.
  kConnected,
  // The default network has been changed.
  kDefaultNetworkChanged,
  kMaxValue = kDefaultNetworkChanged
};

// An interface class to notify the observer of reconnect event. This should be
// implemented when attempting to notify the observer of the reconnect event.
class NET_EXPORT ConnectionChangeNotifier {
 public:
  // Aggregates core connection-establishment statistics (connection protocol,
  // setup latency, and request initiator) to pass onto connection change
  // observers.
  struct NET_EXPORT EstablishedConnectionInfo {
    EstablishedConnectionInfo();
    ~EstablishedConnectionInfo();
    EstablishedConnectionInfo(const EstablishedConnectionInfo& other);
    EstablishedConnectionInfo& operator=(
        const EstablishedConnectionInfo& other);

    NextProto connection_info = NextProto::kProtoUnknown;
    base::TimeDelta connection_setup_time;
    ConnectionEstablishmentInitiator initiator =
        ConnectionEstablishmentInitiator::kRequest;
  };
  // An observer class for the `ConnectionChangeNotifier`. This class will
  // unregister itself from the ObserverList of the notifier when destructing
  // to avoid dangling pointers.
  class NET_EXPORT Observer : public base::CheckedObserver {
   public:
    using ObserverCallback =
        base::OnceCallback<void(const ConnectionChangeNotifier::Observer*)>;

    Observer();
    ~Observer() override;

    // Notify that the underlying network connection has been established.
    virtual void OnConnectionEstablished(
        const EstablishedConnectionInfo& info) = 0;

    // Notify that the underlying network session has been closed.
    virtual void OnSessionClosed(bool was_ever_used_to_create_streams) = 0;

    // Notify that the network connection could not be established.
    virtual void OnConnectionFailed() = 0;

    // Notify on a network change event.
    virtual void OnNetworkEvent(NetworkChangeEvent event) = 0;

    base::WeakPtr<Observer> GetWeakPtr();

   private:
    friend class ConnectionChangeNotifier;

    // Called when the observer has been attached to the notifier. This will
    // pass the `WeakPtr` of the notifier so that the observer can unregister
    // itself on destruct.
    void OnAttach(base::WeakPtr<ConnectionChangeNotifier> notifier);

    base::WeakPtr<ConnectionChangeNotifier> notifier_;

    base::WeakPtrFactory<Observer> weak_factory_{this};
  };

  ConnectionChangeNotifier();
  ~ConnectionChangeNotifier();

  // Notify that the underlying network connection has been established.
  void OnConnectionEstablished(const EstablishedConnectionInfo& info);

  // Notify that the underlying network session has been closed.
  void OnSessionClosed(bool was_ever_used_to_create_streams);

  // Notify that the network connection could not be established.
  void OnConnectionFailed();

  // Notify on a network change event.
  void OnNetworkEvent(NetworkChangeEvent event);

  void AddObserver(ConnectionChangeNotifier::Observer* observer);
  void RemoveObserver(const ConnectionChangeNotifier::Observer* observer);

 private:
  base::ObserverList<ConnectionChangeNotifier::Observer> observer_list_;

  base::WeakPtrFactory<ConnectionChangeNotifier> weak_factory_{this};
};

// Per-session QUIC keep-alive, handshake initial-RTT, and connection-option
// tuning passed to `NetworkContext::PreconnectSockets()`.
struct NET_EXPORT ConnectionKeepAliveConfig {
  ConnectionKeepAliveConfig() = default;
  ~ConnectionKeepAliveConfig() = default;

  ConnectionKeepAliveConfig(const ConnectionKeepAliveConfig& other) = default;
  ConnectionKeepAliveConfig& operator=(const ConnectionKeepAliveConfig& other) =
      default;
  ConnectionKeepAliveConfig(ConnectionKeepAliveConfig&& other) = default;
  ConnectionKeepAliveConfig& operator=(ConnectionKeepAliveConfig&& other) =
      default;

  bool operator==(const ConnectionKeepAliveConfig& other) const = default;

  // Returns true iff all timeout and RTT fields are non-negative, finite, and
  // mutually consistent with QUIC's idle-timeout and keep-alive contracts.
  bool IsValid() const;

  // Timeout for the session to be closed, counted from the last successful
  // packet/PING. Must be zero or in `(2s, UINT32_MAX s]` so that
  // `QuicConfig` storage and `QuicTime` deadline arithmetic remain
  // overflow-safe and `QuicConnection::SetNetworkTimeouts()`'s 1s local client
  // deduction leaves room for a >= 1s keep-alive interval. If zero, the default
  // QUIC idle network timeout is preserved (`ping_interval` must also be zero).
  base::TimeDelta idle_timeout;

  // Interval between keep-alive PINGs, counted from the last ping. Must be zero
  // or `>= 1s` and finite (`QuicPingManager` schedules keep-alive alarms at 1s
  // granularity). When `idle_timeout` is positive, the effective ping interval
  // (`ping_interval` if positive, or QUIC's default `kPingTimeoutSecs` when
  // zero) must be strictly less than `idle_timeout - 1s` so the ping alarm
  // fires before the client's local idle detector alarm.
  base::TimeDelta ping_interval;

  // Enables the connection keep alive mechanism to periodically send PING
  // to the server when there are no active requests.
  bool enable_connection_keep_alive = false;

  // Client-side initial RTT to configure locally on the QUIC connection before
  // any RTT samples are received, used only when no cached SRTT, 2G/3G
  // estimate, or explicit handshake RTT is already configured (and not sent in
  // the `IRTT` transport parameter to the server). This controls the client's
  // initial Probe Timeout (PTO) delay during the QUIC handshake
  // (`3 * initial_rtt`). Must be non-negative and finite. If zero, the default
  // initial RTT is used unchanged.
  base::TimeDelta initial_rtt;

  // The QUIC connection options which will be sent to the server in order to
  // enable certain QUIC features. This should be set using `QuicTag`s (32-bit
  // value represented in ASCII equivalent e.g. EXMP). If we want to set
  // multiple features, then the values should be separated with a comma
  // (e.g. "ABCD,EFGH").
  std::string quic_connection_options;
};

// Keeps track of the connection management relevant information (e.g.
// connection keep alive configs, reconnect notification configs) to be passed
// on to the underlying connection.
struct NET_EXPORT ConnectionManagementConfig {
  ConnectionManagementConfig();
  ~ConnectionManagementConfig();
  ConnectionManagementConfig(const ConnectionManagementConfig& other);
  ConnectionManagementConfig(ConnectionManagementConfig&& other);

  ConnectionManagementConfig& operator=(
      const ConnectionManagementConfig& other) = default;
  ConnectionManagementConfig& operator=(ConnectionManagementConfig&& other) =
      default;

  // Connection keep alive related information.
  std::optional<ConnectionKeepAliveConfig> keep_alive_config;

  // A reference to the `ConnectionChangeNotifier::Observer`.
  base::WeakPtr<ConnectionChangeNotifier::Observer> connection_change_observer;
};

}  // namespace net

#endif  // NET_BASE_RECONNECT_NOTIFIER_H_
