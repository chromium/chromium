// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_H_
#define CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_H_

#include "base/memory/weak_ptr.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {

class SmartCardEmulationManager;

// Implements the SmartCardContext Mojo interface for DevTools emulation.
//
// This class acts as a proxy: it intercepts Mojo calls (like ListReaders)
// and forwards them to the SmartCardEmulationManager.
class EmulatedSmartCardContext : public device::mojom::SmartCardContext {
 public:
  EmulatedSmartCardContext(base::WeakPtr<SmartCardEmulationManager> manager,
                           uint32_t id);

  ~EmulatedSmartCardContext() override;

  EmulatedSmartCardContext(const EmulatedSmartCardContext&) = delete;
  EmulatedSmartCardContext& operator=(const EmulatedSmartCardContext&) = delete;

  void ListReaders(ListReadersCallback callback) override;

  void GetStatusChange(
      base::TimeDelta timeout,
      std::vector<device::mojom::SmartCardReaderStateInPtr> reader_states,
      GetStatusChangeCallback callback) override;

  void Cancel(CancelCallback callback) override;

  void Connect(
      const std::string& reader,
      device::mojom::SmartCardShareMode share_mode,
      device::mojom::SmartCardProtocolsPtr preferred_protocols,
      mojo::PendingRemote<device::mojom::SmartCardConnectionWatcher> watcher,
      ConnectCallback callback) override;

 private:
  base::WeakPtr<SmartCardEmulationManager> manager_;
  const uint32_t id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_H_
