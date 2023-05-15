// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/mock_smart_card_context_factory.h"

using device::mojom::SmartCardCreateContextResult;
using device::mojom::SmartCardProtocol;
using device::mojom::SmartCardShareMode;
using testing::_;

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

void MockSmartCardContextFactory::ExpectConnectFakeReaderSharedT1(
    mojo::Receiver<device::mojom::SmartCardConnection>& connection_receiver) {
  EXPECT_CALL(*this, Connect("Fake reader", SmartCardShareMode::kShared, _, _))
      .WillOnce([&connection_receiver](
                    const std::string& reader,
                    device::mojom::SmartCardShareMode share_mode,
                    device::mojom::SmartCardProtocolsPtr preferred_protocols,
                    SmartCardContext::ConnectCallback callback) {
        EXPECT_FALSE(preferred_protocols->t0);
        EXPECT_TRUE(preferred_protocols->t1);
        EXPECT_FALSE(preferred_protocols->raw);

        auto success = device::mojom::SmartCardConnectSuccess::New(
            connection_receiver.BindNewPipeAndPassRemote(),
            SmartCardProtocol::kT1);

        std::move(callback).Run(
            device::mojom::SmartCardConnectResult::NewSuccess(
                std::move(success)));
      });
}

}  // namespace content
