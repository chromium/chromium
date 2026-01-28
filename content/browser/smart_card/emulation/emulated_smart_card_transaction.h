// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_TRANSACTION_H_
#define CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_TRANSACTION_H_

#include "base/memory/weak_ptr.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {

class SmartCardEmulationManager;

class EmulatedSmartCardTransaction
    : public device::mojom::SmartCardTransaction {
 public:
  EmulatedSmartCardTransaction(base::WeakPtr<SmartCardEmulationManager> manager,
                               const uint32_t handle);

  ~EmulatedSmartCardTransaction() override;

  EmulatedSmartCardTransaction(const EmulatedSmartCardTransaction&) = delete;
  EmulatedSmartCardTransaction& operator=(const EmulatedSmartCardTransaction&) =
      delete;

  void EndTransaction(device::mojom::SmartCardDisposition disposition,
                      EndTransactionCallback callback) override;

 private:
  base::WeakPtr<SmartCardEmulationManager> manager_;
  const uint32_t handle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_TRANSACTION_H_
