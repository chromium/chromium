// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/message_view.h"

#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace IPC {

MessageView::MessageView() = default;

MessageView::MessageView(
    base::span<const uint8_t> bytes,
    std::optional<std::vector<mojo::native::SerializedHandlePtr>> handles)
    : bytes_(bytes), handles_(std::move(handles)) {}

MessageView::MessageView(MessageView&&) = default;

MessageView::~MessageView() = default;

MessageView& MessageView::operator=(MessageView&&) = default;

std::optional<std::vector<mojo::native::SerializedHandlePtr>>
MessageView::TakeHandles() {
  return std::move(handles_);
}

}  // namespace IPC
