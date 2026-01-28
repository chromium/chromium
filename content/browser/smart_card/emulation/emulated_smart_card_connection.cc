// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_connection.h"

#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"

namespace content {

namespace {
using device::mojom::SmartCardError;
}  // namespace

EmulatedSmartCardConnection::EmulatedSmartCardConnection(
    base::WeakPtr<SmartCardEmulationManager> manager,
    const uint32_t handle)
    : manager_(std::move(manager)), handle_(handle) {}

EmulatedSmartCardConnection::~EmulatedSmartCardConnection() = default;

void EmulatedSmartCardConnection::Disconnect(
    device::mojom::SmartCardDisposition disposition,
    DisconnectCallback callback) {
  if (!manager_) {
    // If the manager is gone, the connection is effectively already closed.
    // Return success to ensure clean teardown on the client side.
    std::move(callback).Run(device::mojom::SmartCardResult::NewSuccess(
        device::mojom::SmartCardSuccess::kOk));
    return;
  }
  manager_->OnDisconnect(handle_, disposition, std::move(callback));
}

void EmulatedSmartCardConnection::Transmit(
    device::mojom::SmartCardProtocol protocol,
    const std::vector<uint8_t>& data,
    TransmitCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

void EmulatedSmartCardConnection::Control(uint32_t control_code,
                                          const std::vector<uint8_t>& data,
                                          ControlCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

void EmulatedSmartCardConnection::GetAttrib(uint32_t id,
                                            GetAttribCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardDataResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

void EmulatedSmartCardConnection::SetAttrib(uint32_t id,
                                            const std::vector<uint8_t>& data,
                                            SetAttribCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

void EmulatedSmartCardConnection::Status(StatusCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardStatusResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

void EmulatedSmartCardConnection::BeginTransaction(
    BeginTransactionCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardTransactionResult::NewError(
        SmartCardError::kServiceStopped));
    return;
  }
}

}  // namespace content
