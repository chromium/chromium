// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MESSAGE_VIEW_H_
#define IPC_MESSAGE_VIEW_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom-forward.h"

namespace IPC {

class COMPONENT_EXPORT(IPC_MOJOM) MessageView {
 public:
  MessageView();
  MessageView(
      const Message& message,
      base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles);
  MessageView(
      mojo_base::BigBufferView buffer_view,
      base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles);
  MessageView(MessageView&&);
  ~MessageView();

  MessageView& operator=(MessageView&&);

  const char* data() const {
    return reinterpret_cast<const char*>(buffer_view_.data().data());
  }

  uint32_t size() const {
    return static_cast<uint32_t>(buffer_view_.data().size());
  }

  mojo_base::BigBufferView TakeBufferView() { return std::move(buffer_view_); }

  base::Optional<std::vector<mojo::native::SerializedHandlePtr>> TakeHandles();

 private:
  mojo_base::BigBufferView buffer_view_;
  base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace IPC

#endif  // IPC_MESSAGE_VIEW_H_
