// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_TEST_FAKE_NETWORK_MANAGER_H_
#define REMOTING_TEST_FAKE_NETWORK_MANAGER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "third_party/webrtc/rtc_base/network.h"

namespace remoting {

// FakeNetworkManager always returns one interface with the IP address
// specified in the constructor.
class FakeNetworkManager : public webrtc::NetworkManager {
 public:
  explicit FakeNetworkManager(const webrtc::IPAddress& address);
  ~FakeNetworkManager() override;

  // webrtc::NetworkManager interface.
  void StartUpdating() override;
  void StopUpdating() override;
  std::vector<const webrtc::Network*> GetNetworks() const override;
  std::vector<const webrtc::Network*> GetAnyAddressNetworks() override;

 protected:
  void SendNetworksChangedSignal();

  bool started_;
  std::unique_ptr<webrtc::Network> network_;

  base::WeakPtrFactory<FakeNetworkManager> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_TEST_FAKE_NETWORK_MANAGER_H_
