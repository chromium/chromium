// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_context.h"

#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"

namespace content {

namespace {

using device::mojom::SmartCardConnectResult;
using device::mojom::SmartCardError;
using device::mojom::SmartCardListReadersResult;
using device::mojom::SmartCardResult;
using device::mojom::SmartCardStatusChangeResult;
using device::mojom::SmartCardSuccess;

}  // namespace

EmulatedSmartCardContext::EmulatedSmartCardContext(
    base::WeakPtr<SmartCardEmulationManager> manager,
    uint32_t id)
    : manager_(std::move(manager)), id_(id) {}

EmulatedSmartCardContext::~EmulatedSmartCardContext() {
  if (manager_) {
    manager_->OnReleaseContext(id_);
  }
}

void EmulatedSmartCardContext::ListReaders(ListReadersCallback callback) {
  if (!manager_) {
    std::move(callback).Run(device::mojom::SmartCardListReadersResult::NewError(
        device::mojom::SmartCardError::kNoService));
    return;
  }
  manager_->OnListReaders(id_, std::move(callback));
}

void EmulatedSmartCardContext::Connect(
    const std::string& reader,
    device::mojom::SmartCardShareMode share_mode,
    device::mojom::SmartCardProtocolsPtr preferred_protocols,
    mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
    ConnectCallback callback) {
  if (!manager_) {
    std::move(callback).Run(
        SmartCardConnectResult::NewError(SmartCardError::kNoService));
    return;
  }
  manager_->OnConnect(id_, reader, share_mode, std::move(preferred_protocols),
                      std::move(watcher), std::move(callback));
}

void EmulatedSmartCardContext::GetStatusChange(
    base::TimeDelta timeout,
    std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
    GetStatusChangeCallback callback) {
  if (!manager_) {
    std::move(callback).Run(
        SmartCardStatusChangeResult::NewError(SmartCardError::kNoService));
    return;
  }
  manager_->OnGetStatusChange(id_, timeout, std::move(reader_states),
                              std::move(callback));
}

void EmulatedSmartCardContext::Cancel(CancelCallback callback) {
  if (!manager_) {
    std::move(callback).Run(
        SmartCardResult::NewError(SmartCardError::kNoService));
    return;
  }
  manager_->OnCancel(id_, std::move(callback));
}

}  // namespace content
