// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_widget_impl.h"

namespace content {

MockWidgetImpl::MockWidgetImpl(mojo::PendingReceiver<mojom::Widget> request)
    : receiver_(this, std::move(request)) {}

MockWidgetImpl::~MockWidgetImpl() {}

void MockWidgetImpl::SetupWidgetInputHandler(
    mojo::PendingReceiver<mojom::WidgetInputHandler> receiver,
    mojo::PendingRemote<mojom::WidgetInputHandlerHost> host) {
  input_handler_ = std::make_unique<MockWidgetInputHandler>(std::move(receiver),
                                                            std::move(host));
}

}  // namespace content
