// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_FACTORY_H_
#define CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_FACTORY_H_

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/smart_card/emulation/smart_card_emulation_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/device/public/mojom/smart_card.mojom.h"

namespace content {

// Intercepts Mojo calls from the browser and forwards them to the DevTools
// Handler. It keeps the Mojo callbacks alive in memory until the DevTools
// Frontend provides a response.
class EmulatedSmartCardContextFactory
    : public device::mojom::SmartCardContextFactory {
 public:
  // The handler must outlive this factory.
  explicit EmulatedSmartCardContextFactory(SmartCardEmulationManager& manager);
  ~EmulatedSmartCardContextFactory() override;

  // Mojo Interface Implementation.
  void CreateContext(CreateContextCallback callback) override;

  // Helper to create a new pipe to this factory.
  void BindReceiver(
      mojo::PendingReceiver<device::mojom::SmartCardContextFactory> receiver);

  base::WeakPtr<EmulatedSmartCardContextFactory> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // References the SmartCardEmulationManager instance that owns this factory.
  // Since the manager owns this factory via std::unique_ptr, it is
  // guaranteed to outlive this factory instance.
  raw_ref<SmartCardEmulationManager> manager_;

  mojo::ReceiverSet<device::mojom::SmartCardContextFactory> receivers_;

  base::WeakPtrFactory<EmulatedSmartCardContextFactory> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMART_CARD_EMULATION_EMULATED_SMART_CARD_CONTEXT_FACTORY_H_
