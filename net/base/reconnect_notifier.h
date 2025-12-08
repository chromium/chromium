// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_RECONNECT_NOTIFIER_H_
#define NET_BASE_RECONNECT_NOTIFIER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "net/base/net_export.h"

namespace net {
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
  // An observer class for the `ConnectionChangeNotifier`. This class will
  // unregister itself from the ObserverList of the notifier when destructing
  // to avoid dangling pointers.
  class NET_EXPORT Observer : public base::CheckedObserver {
   public:
    using ObserverCallback =
        base::OnceCallback<void(const ConnectionChangeNotifier::Observer*)>;

    Observer();
    ~Observer() override;

    // Notify that the underlying network session has been closed.
    virtual void OnSessionClosed() = 0;

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

  // Notify that the underlying network session has been closed.
  void OnSessionClosed();

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

// Keeps track of the relevant information to conduct connection keep-alive.
struct NET_EXPORT ConnectionKeepAliveConfig {
  ConnectionKeepAliveConfig() = default;
  ~ConnectionKeepAliveConfig() = default;

  ConnectionKeepAliveConfig(const ConnectionKeepAliveConfig& other) = default;
  ConnectionKeepAliveConfig& operator=(const ConnectionKeepAliveConfig& other) =
      default;
  ConnectionKeepAliveConfig(ConnectionKeepAliveConfig&& other) = default;
  ConnectionKeepAliveConfig& operator=(ConnectionKeepAliveConfig&& other) =
      default;

  // Timeout for the session to be closed. Counted from the last successful
  // PING. This is kept as signed integer since it will be later passed as
  // a time offset of the underlying time representation such as `QuicTime`.
  int32_t idle_timeout_in_seconds = 0;

  // Interval between two pings. Counted from the last ping. This should be
  // reasonably shorter than `idle_timeout` so that a PING frame can be
  // exchanged before the idle timeout. This is kept as signed integer since it
  // will be later passed as a time offset of the underlying time representation
  // such as `QuicTime`.
  int32_t ping_interval_in_seconds = 0;

  // Enables the connection keep alive mechanism to periodically send PING
  // to the server when there are no active requests.
  bool enable_connection_keep_alive = false;

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
