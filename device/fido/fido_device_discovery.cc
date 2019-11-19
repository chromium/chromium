// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_discovery.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"

namespace device {

FidoDeviceDiscovery::Observer::~Observer() = default;

FidoDeviceDiscovery::FidoDeviceDiscovery(FidoTransportProtocol transport)
    : FidoDiscoveryBase(transport) {}

FidoDeviceDiscovery::~FidoDeviceDiscovery() = default;

void FidoDeviceDiscovery::Start() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kStarting;

  // To ensure that that NotifyStarted() is never invoked synchronously,
  // post task asynchronously.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FidoDeviceDiscovery::StartInternal,
                                weak_factory_.GetWeakPtr()));
}

void FidoDeviceDiscovery::NotifyDiscoveryStarted(bool success) {
  DCHECK_EQ(state_, State::kStarting);
  if (success)
    state_ = State::kRunning;
  if (!observer())
    return;

  std::vector<FidoAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  observer()->DiscoveryStarted(this, success, std::move(authenticators));
}

void FidoDeviceDiscovery::NotifyAuthenticatorAdded(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer() || state_ != State::kRunning)
    return;
  observer()->AuthenticatorAdded(this, authenticator);
}

void FidoDeviceDiscovery::NotifyAuthenticatorRemoved(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer() || state_ != State::kRunning)
    return;
  observer()->AuthenticatorRemoved(this, authenticator);
}

std::vector<FidoDeviceAuthenticator*>
FidoDeviceDiscovery::GetAuthenticatorsForTesting() {
  std::vector<FidoDeviceAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

std::vector<const FidoDeviceAuthenticator*>
FidoDeviceDiscovery::GetAuthenticatorsForTesting() const {
  std::vector<const FidoDeviceAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

FidoDeviceAuthenticator* FidoDeviceDiscovery::GetAuthenticatorForTesting(
    base::StringPiece authenticator_id) {
  return GetAuthenticator(authenticator_id);
}

FidoDeviceAuthenticator* FidoDeviceDiscovery::GetAuthenticator(
    base::StringPiece authenticator_id) {
  auto found = authenticators_.find(authenticator_id);
  return found != authenticators_.end() ? found->second.get() : nullptr;
}

bool FidoDeviceDiscovery::AddDevice(std::unique_ptr<FidoDevice> device) {
  auto authenticator =
      std::make_unique<FidoDeviceAuthenticator>(std::move(device));
  const auto result =
      authenticators_.emplace(authenticator->GetId(), std::move(authenticator));
  if (!result.second) {
    return false;  // Duplicate device id.
  }

  NotifyAuthenticatorAdded(result.first->second.get());
  return true;
}

bool FidoDeviceDiscovery::RemoveDevice(base::StringPiece device_id) {
  auto found = authenticators_.find(device_id);
  if (found == authenticators_.end())
    return false;

  auto authenticator = std::move(found->second);
  authenticators_.erase(found);
  NotifyAuthenticatorRemoved(authenticator.get());
  return true;
}

}  // namespace device
