// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/message_view.h"

#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace IPC {

MessageView::MessageView() = default;

MessageView::MessageView(
    const Message& message,
    base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles)
    : buffer_view_(base::make_span(static_cast<const uint8_t*>(message.data()),
                                   message.size())),
      handles_(std::move(handles)) {}

MessageView::MessageView(
    mojo_base::BigBufferView buffer_view,
    base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles)
    : buffer_view_(std::move(buffer_view)), handles_(std::move(handles)) {}

MessageView::MessageView(MessageView&&) = default;

MessageView::~MessageView() = default;

MessageView& MessageView::operator=(MessageView&&) = default;

base::Optional<std::vector<mojo::native::SerializedHandlePtr>>
MessageView::TakeHandles() {
  return std::move(handles_);
}

}  // namespace IPC
