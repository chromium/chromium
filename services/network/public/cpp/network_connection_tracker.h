// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONNECTION_TRACKER_H_
#define SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONNECTION_TRACKER_H_

#include <list>
#include <memory>

#include "base/atomicops.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list_threadsafe.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace network {

// Defines the type of a callback that will return a NetworkConnectionTracker
// instance.
class NetworkConnectionTracker;
using NetworkConnectionTrackerGetter =
    base::RepeatingCallback<NetworkConnectionTracker*()>;
// Defines the type of a callback that can be used to asynchronously get a
// NetworkConnectionTracker instance.
using NetworkConnectionTrackerAsyncGetter = base::RepeatingCallback<void(
    base::OnceCallback<void(NetworkConnectionTracker*)>)>;

// This class subscribes to network change events from
// network::mojom::NetworkChangeManager and propogates these notifications to
// its NetworkConnectionObservers registered through
// AddNetworkConnectionObserver()/RemoveNetworkConnectionObserver().
class COMPONENT_EXPORT(NETWORK_CPP) NetworkConnectionTracker
    : public network::mojom::NetworkChangeManagerClient {
 public:
  using BindingCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::NetworkChangeManager>)>;
  using ConnectionTypeCallback =
      base::OnceCallback<void(network::mojom::ConnectionType)>;

  class COMPONENT_EXPORT(NETWORK_CPP) NetworkConnectionObserver {
   public:
    // Please refer to NetworkChangeManagerClient::OnNetworkChanged for when
    // this method is invoked.
    virtual void OnConnectionChanged(network::mojom::ConnectionType type) = 0;

   protected:
    virtual ~NetworkConnectionObserver() {}
  };

  // Constructs a NetworkConnectionTracker. |callback| should bind the given
  // mojo::PendingReceiver<NetworkChangeManager> to the NetworkChangeManager
  // that should be used. NetworkConnectionTracker does not need to be destroyed
  // before the network service.
  explicit NetworkConnectionTracker(BindingCallback callback);

  NetworkConnectionTracker(const NetworkConnectionTracker&) = delete;
  NetworkConnectionTracker& operator=(const NetworkConnectionTracker&) = delete;

  ~NetworkConnectionTracker() override;

  // If connection type can be retrieved synchronously, returns true and |type|
  // will contain the current connection type, and |callback| will not be
  // called; Otherwise, returns false and does not modify |type|, in which
  // case, |callback| will be called on the calling thread when connection type
  // is ready. The connection type being available does not imply it is not
  // CONNECTION_UNKNKOWN. This method is thread safe. Please also refer to
  // net::NetworkChangeNotifier::GetConnectionType() for documentation.
  virtual bool GetConnectionType(network::mojom::ConnectionType* type,
                                 ConnectionTypeCallback callback);

  // Returns true if the network is currently in an offline or unknown state.
  bool IsOffline() const;

  // Returns true if |type| is a cellular connection.
  // Returns false if |type| is CONNECTION_UNKNOWN, and thus, depending on the
  // implementation of GetConnectionType(), it is possible that
  // IsConnectionCellular(GetConnectionType()) returns false even if the
  // current connection is cellular.
  static bool IsConnectionCellular(network::mojom::ConnectionType type);

  // Registers |observer| to receive notifications of network changes. The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  //
  // Observers registered with this method are required to be unregistered
  // before the NetworkConnectionTracker is deleted.
  void AddNetworkConnectionObserver(NetworkConnectionObserver* observer);

  // Registers |observer| to receive notifications of network changes. The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  //
  // Observers registered with this method are not expected to be unregistered.
  // This should only be used by leaky singletons to avoid an error on shutdown
  // about observers not being unregistered.
  void AddLeakyNetworkConnectionObserver(NetworkConnectionObserver* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddNetworkConnectionObserver() was called.
  // All observers must be unregistered before |this| is destroyed.
  void RemoveNetworkConnectionObserver(NetworkConnectionObserver* observer);

 protected:
  // Constructor used in testing to mock out network service.
  NetworkConnectionTracker();

  // NetworkChangeManagerClient implementation. Protected for testing.
  void OnInitialConnectionType(network::mojom::ConnectionType type) override;
  void OnNetworkChanged(network::mojom::ConnectionType type) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NetworkGetConnectionTest,
                           GetConnectionTypeOnDifferentThread);

  // Starts listening for connection change notifications from
  // |network_service|. Observers may be added and GetConnectionType called, but
  // no network information will be provided until this method is called. For
  // unit tests, this class can be subclassed, and OnInitialConnectionType /
  // OnNetworkChanged may be called directly, instead of providing a
  // NetworkService.
  void Initialize();

  // Serves as a connection error handler, and is invoked when network service
  // restarts.
  void HandleNetworkServicePipeBroken();

  // Callback to bind a mojo::PendingReceiver<NetworkChangeManager>.
  const BindingCallback bind_receiver_callback_;

  // The task runner that |this| lives on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Protect access to |connection_type_callbacks_|.
  base::Lock lock_;

  // Saves user callback if GetConnectionType() cannot complete synchronously.
  std::list<ConnectionTypeCallback> connection_type_callbacks_;

  // |connection_type_| is set on one thread but read on many threads.
  // The default value is -1 before OnInitialConnectionType().
  base::subtle::Atomic32 connection_type_;

  const scoped_refptr<base::ObserverListThreadSafe<NetworkConnectionObserver>>
      network_change_observer_list_;
  const scoped_refptr<base::ObserverListThreadSafe<NetworkConnectionObserver>>
      leaky_network_change_observer_list_;

  mojo::Receiver<network::mojom::NetworkChangeManagerClient> receiver_{this};

  // Only the initialization and re-initialization of |this| are required to
  // be bound to the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

namespace base {

template <>
struct ScopedObservationTraits<
    network::NetworkConnectionTracker,
    network::NetworkConnectionTracker::NetworkConnectionObserver> {
  static void AddObserver(
      network::NetworkConnectionTracker* source,
      network::NetworkConnectionTracker::NetworkConnectionObserver* observer) {
    source->AddNetworkConnectionObserver(observer);
  }
  static void RemoveObserver(
      network::NetworkConnectionTracker* source,
      network::NetworkConnectionTracker::NetworkConnectionObserver* observer) {
    source->RemoveNetworkConnectionObserver(observer);
  }
};

}  // namespace base

#endif  // SERVICES_NETWORK_PUBLIC_CPP_NETWORK_CONNECTION_TRACKER_H_
