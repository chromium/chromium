// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_EMULATION_SMART_CARD_EMULATION_MANAGER_H_
#define CONTENT_BROWSER_SMART_CARD_EMULATION_SMART_CARD_EMULATION_MANAGER_H_

#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {

// Interface for managing smart card emulation requests.
// Implemented by SmartCardEmulationHandler (DevTools) and mocked in tests.
class SmartCardEmulationManager {
 public:
  virtual ~SmartCardEmulationManager() = default;

  virtual void OnCreateContext(
      device::mojom::SmartCardContextFactory::CreateContextCallback
          callback) = 0;

  virtual void OnListReaders(
      uint32_t context_id,
      device::mojom::SmartCardContext::ListReadersCallback callback) = 0;

  virtual void OnGetStatusChange(
      uint32_t context_id,
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      device::mojom::SmartCardContext::GetStatusChangeCallback callback) = 0;

  virtual void OnCancel(
      uint32_t context_id,
      device::mojom::SmartCardContext::CancelCallback callback) = 0;

  virtual void OnConnect(
      uint32_t context_id,
      const std::string& reader,
      device::mojom::SmartCardShareMode share_mode,
      device::mojom::SmartCardProtocolsPtr preferred_protocols,
      mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
      device::mojom::SmartCardContext::ConnectCallback callback) = 0;

  virtual void OnDisconnect(
      uint32_t connection_id,
      device::mojom::SmartCardDisposition disposition,
      device::mojom::SmartCardConnection::DisconnectCallback callback) = 0;

  virtual void OnControl(
      uint32_t connection_id,
      uint32_t control_code,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::ControlCallback callback) = 0;

  virtual void OnGetAttrib(
      uint32_t connection_id,
      uint32_t id,
      device::mojom::SmartCardConnection::GetAttribCallback callback) = 0;

  virtual void OnTransmit(
      uint32_t connection_id,
      device::mojom::SmartCardProtocol protocol,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::TransmitCallback callback) = 0;

  virtual void OnStatus(
      uint32_t connection_id,
      device::mojom::SmartCardConnection::StatusCallback callback) = 0;

  virtual void OnSetAttrib(
      uint32_t connection_id,
      uint32_t id,
      const std::vector<uint8_t>& data,
      device::mojom::SmartCardConnection::SetAttribCallback callback) = 0;

  virtual void OnBeginTransaction(
      uint32_t connection_id,
      device::mojom::SmartCardConnection::BeginTransactionCallback
          callback) = 0;

  virtual void OnEndTransaction(
      uint32_t connection_id,
      device::mojom::SmartCardDisposition disposition,
      device::mojom::SmartCardTransaction::EndTransactionCallback callback) = 0;

  virtual void OnReleaseContext(uint32_t context_id) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_EMULATION_SMART_CARD_EMULATION_MANAGER_H_
