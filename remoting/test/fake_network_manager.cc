// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/fake_network_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "jingle/glue/utils.h"
#include "third_party/webrtc/rtc_base/socket_address.h"

namespace remoting {

FakeNetworkManager::FakeNetworkManager(const rtc::IPAddress& address)
    : started_(false) {
  network_.reset(new rtc::Network("fake", "Fake Network", address, 32));
  network_->AddIP(address);
}

FakeNetworkManager::~FakeNetworkManager() = default;

void FakeNetworkManager::StartUpdating() {
  started_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeNetworkManager::SendNetworksChangedSignal,
                                weak_factory_.GetWeakPtr()));
}

void FakeNetworkManager::StopUpdating() {
  started_ = false;
}

void FakeNetworkManager::GetNetworks(NetworkList* networks) const {
  networks->clear();
  networks->push_back(network_.get());
}

void FakeNetworkManager::SendNetworksChangedSignal() {
  SignalNetworksChanged();
}

}  // namespace remoting
