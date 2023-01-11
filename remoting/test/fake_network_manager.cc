// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_network_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "components/webrtc/net_address_utils.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {

FakeNetworkManager::FakeNetworkManager(const rtc::IPAddress& address)
    : started_(false) {
  network_ =
      std::make_unique<rtc::Network>("fake", "Fake Network", address, 32);
  network_->AddIP(address);
}

FakeNetworkManager::~FakeNetworkManager() = default;

void FakeNetworkManager::StartUpdating() {
  started_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeNetworkManager::SendNetworksChangedSignal,
                                weak_factory_.GetWeakPtr()));
}

void FakeNetworkManager::StopUpdating() {
  started_ = false;
}

std::vector<const rtc::Network*> FakeNetworkManager::GetNetworks() const {
  return {network_.get()};
}

std::vector<const rtc::Network*> FakeNetworkManager::GetAnyAddressNetworks() {
  return {};
}

void FakeNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace remoting
