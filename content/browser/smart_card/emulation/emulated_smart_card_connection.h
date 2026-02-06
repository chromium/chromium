// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONNECTION_H_
#define CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONNECTION_H_

#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {

class SmartCardEmulationManager;

// Implements the SmartCardConnection Mojo interface for DevTools emulation.
//
// Represents an active connection to a specific emulated smart card.
class EmulatedSmartCardConnection : public device::mojom::SmartCardConnection {
 public:
  EmulatedSmartCardConnection(
      base::WeakPtr<SmartCardEmulationManager> manager,
      const uint32_t handle,
      mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher);

  ~EmulatedSmartCardConnection() override;

  EmulatedSmartCardConnection(const EmulatedSmartCardConnection&) = delete;
  EmulatedSmartCardConnection& operator=(const EmulatedSmartCardConnection&) =
      delete;

  void Disconnect(device::mojom::SmartCardDisposition disposition,
                  DisconnectCallback callback) override;

  void Transmit(device::mojom::SmartCardProtocol protocol,
                const std::vector<uint8_t>& data,
                TransmitCallback callback) override;
  void Control(uint32_t control_code,
               const std::vector<uint8_t>& data,
               ControlCallback callback) override;

  void GetAttrib(uint32_t id, GetAttribCallback callback) override;

  void SetAttrib(uint32_t id,
                 const std::vector<uint8_t>& data,
                 SetAttribCallback callback) override;

  void Status(StatusCallback callback) override;

  void BeginTransaction(BeginTransactionCallback callback) override;

 private:
  void NotifyWatcher();

  base::WeakPtr<SmartCardEmulationManager> manager_;
  const uint32_t handle_;
  mojo::Remote<device::mojom::SmartCardConnectionWatcher> watcher_remote_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONNECTION_H_
