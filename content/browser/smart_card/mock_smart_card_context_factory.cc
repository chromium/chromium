// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/mock_smart_card_context_factory.h"

using device::mojom::SmartCardCreateContextResult;

namespace content {

MockSmartCardContextFactory::MockSmartCardContextFactory() = default;
MockSmartCardContextFactory::~MockSmartCardContextFactory() = default;

mojo::PendingRemote<device::mojom::SmartCardContextFactory>
MockSmartCardContextFactory::GetRemote() {
  mojo::PendingRemote<device::mojom::SmartCardContextFactory> pending_remote;
  receivers_.Add(this, pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

void MockSmartCardContextFactory::CreateContext(
    CreateContextCallback callback) {
  mojo::PendingRemote<device::mojom::SmartCardContext> context_remote;
  context_receivers_.Add(this, context_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(
      SmartCardCreateContextResult::NewContext(std::move(context_remote)));
}

}  // namespace content
