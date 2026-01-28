// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/emulation/emulated_smart_card_context_factory.h"

#include "content/browser/smart_card/emulation/emulated_smart_card_context.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

EmulatedSmartCardContextFactory::EmulatedSmartCardContextFactory(
    SmartCardEmulationManager& manager)
    : manager_(manager) {}

EmulatedSmartCardContextFactory::~EmulatedSmartCardContextFactory() {}

void EmulatedSmartCardContextFactory::BindReceiver(
    mojo::PendingReceiver<device::mojom::SmartCardContextFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void EmulatedSmartCardContextFactory::CreateContext(
    CreateContextCallback callback) {
  manager_->OnCreateContext(std::move(callback));
}

}  // namespace content
