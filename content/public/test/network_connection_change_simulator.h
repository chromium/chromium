// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_NETWORK_CONNECTION_CHANGE_SIMULATOR_H_
#define CONTENT_PUBLIC_TEST_NETWORK_CONNECTION_CHANGE_SIMULATOR_H_

#include "build/chromeos_buildflags.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace base {
class RunLoop;
}

namespace content {

// A class to help tests set the network connection type.
class NetworkConnectionChangeSimulator
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  NetworkConnectionChangeSimulator();

  NetworkConnectionChangeSimulator(const NetworkConnectionChangeSimulator&) =
      delete;
  NetworkConnectionChangeSimulator& operator=(
      const NetworkConnectionChangeSimulator&) = delete;

  ~NetworkConnectionChangeSimulator() override;

#if BUILDFLAG(IS_CHROMEOS)
  // Initializes the ChromeOS network connection type.
  // This should be used in tests that don't have a DBus set up.
  void InitializeChromeosConnectionType();
#endif

  // Synchronously sets the connection type.
  void SetConnectionType(network::mojom::ConnectionType connection_type);

 private:
  static void SimulateNetworkChange(network::mojom::ConnectionType type);

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_NETWORK_CONNECTION_CHANGE_SIMULATOR_H_
