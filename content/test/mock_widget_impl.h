// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_WIDGET_IMPL_H_
#define CONTENT_TEST_MOCK_WIDGET_IMPL_H_

#include "content/common/widget.mojom.h"
#include "content/test/mock_widget_input_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace content {

class MockWidgetImpl : public mojom::Widget {
 public:
  explicit MockWidgetImpl(mojo::PendingReceiver<mojom::Widget> receiver);
  ~MockWidgetImpl() override;

  void SetupWidgetInputHandler(
      mojo::PendingReceiver<mojom::WidgetInputHandler> receiver,
      mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) override;

  MockWidgetInputHandler* input_handler() { return input_handler_.get(); }

 private:
  mojo::Receiver<mojom::Widget> receiver_;
  std::unique_ptr<MockWidgetInputHandler> input_handler_;

  DISALLOW_COPY_AND_ASSIGN(MockWidgetImpl);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_WIDGET_IMPL_H_
