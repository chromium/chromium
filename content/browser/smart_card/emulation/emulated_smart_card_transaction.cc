// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_transaction.h"

#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"

namespace content {

EmulatedSmartCardTransaction::EmulatedSmartCardTransaction(
    base::WeakPtr<SmartCardEmulationManager> manager,
    const uint32_t handle)
    : manager_(std::move(manager)), handle_(handle) {}

EmulatedSmartCardTransaction::~EmulatedSmartCardTransaction() = default;

void EmulatedSmartCardTransaction::EndTransaction(
    device::mojom::SmartCardDisposition disposition,
    EndTransactionCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardResult::NewError(
        device::mojom::SmartCardError::kServiceStopped));
    return;
  }
  manager_->OnEndTransaction(handle_, disposition, std::move(callback));
}

}  // namespace content
