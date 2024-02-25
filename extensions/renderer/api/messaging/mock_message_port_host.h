// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_API_MESSAGING_MOCK_MESSAGE_PORT_HOST_H_
#define EXTENSIONS_RENDERER_API_MESSAGING_MOCK_MESSAGE_PORT_HOST_H_

#include "extensions/common/mojom/message_port.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {

class MockMessagePortHost : public mojom::MessagePortHost {
 public:
  MockMessagePortHost();
  ~MockMessagePortHost() override;

  MOCK_METHOD1(ClosePort, void(bool close_channel));
  MOCK_METHOD1(PostMessage, void(Message message));
  MOCK_METHOD0(ResponsePending, void());

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::MessagePortHost> receiver);

 private:
  mojo::AssociatedReceiver<mojom::MessagePortHost> receiver_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_API_MESSAGING_MOCK_MESSAGE_PORT_HOST_H_
