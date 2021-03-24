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
#include "mojo/public/interfaces/bindings/native_struct.mojom-forward.h"

namespace IPC {

class COMPONENT_EXPORT(IPC_MOJOM) MessageView {
 public:
  MessageView();
  MessageView(
      base::span<const uint8_t> bytes,
      base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles);
  MessageView(MessageView&&);
  ~MessageView();

  MessageView& operator=(MessageView&&);

  base::span<const uint8_t> bytes() const { return bytes_; }
  base::Optional<std::vector<mojo::native::SerializedHandlePtr>> TakeHandles();

 private:
  base::span<const uint8_t> bytes_;
  base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

}  // namespace IPC

#endif  // IPC_MESSAGE_VIEW_H_
