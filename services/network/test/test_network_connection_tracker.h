// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_TEST_TEST_NETWORK_CONNECTION_TRACKER_H_
#define SERVICES_NETWORK_TEST_TEST_NETWORK_CONNECTION_TRACKER_H_

#include "services/network/public/cpp/network_connection_tracker.h"

namespace network {

// Allows unit tests to set the network connection type.
// GetConnectionType() can be set to respond synchronously or asynchronously,
// so that it may be tested that tested units are able to correctly handle
// either.
class TestNetworkConnectionTracker : public NetworkConnectionTracker {
 public:
  // Creates and returns a new TestNetworkConnectionTracker instance.
  // The instance is owned by the caller of this function, and there can only
  // be one live instance at a time.
  // This is intended to be called towards the beginning of each test suite.
  static std::unique_ptr<TestNetworkConnectionTracker> CreateInstance();

  // Returns the currently active TestNetworkConnectionTracker instance.
  // CreateInstance() must have been called before calling this.
  static TestNetworkConnectionTracker* GetInstance();

  // Returns true if TestNetworkConnectionTracker instance already exists.
  // It means that CreateInstance() was already called.
  static bool HasInstance();

  // Creates a NetworkConnectionTrackerGetter that will return the active
  // TestNetworkConnectionTracker instance when called.
  static NetworkConnectionTrackerGetter CreateGetter();

  // Creates a NetworkConnectionTrackerGetter that will asynchronously return
  // the active TestNetworkConnectionTracker instance.
  static NetworkConnectionTrackerAsyncGetter CreateAsyncGetter();

  ~TestNetworkConnectionTracker() override;

  bool GetConnectionType(network::mojom::ConnectionType* type,
                         ConnectionTypeCallback callback) override;

  // Sets the current connection type and notifies all observers.
  void SetConnectionType(network::mojom::ConnectionType);

  // Sets whether or not GetConnectionType() will respond synchronously.
  void SetRespondSynchronously(bool respond_synchronously);

 private:
  TestNetworkConnectionTracker();

  // Whether GetConnectionType() will respond synchronously.
  bool respond_synchronously_ = true;
};

}  // namespace network

#endif  // SERVICES_NETWORK_TEST_TEST_NETWORK_CONNECTION_TRACKER_H_
