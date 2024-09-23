// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MESSAGE_MOJOM_TRAITS_H_
#define IPC_MESSAGE_MOJOM_TRAITS_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "ipc/ipc.mojom-shared.h"
#include "ipc/message_view.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/interfaces/bindings/native_struct.mojom.h"

namespace mojo {

template <>
class StructTraits<IPC::mojom::MessageDataView, IPC::MessageView> {
 public:
  static base::span<const uint8_t> bytes(IPC::MessageView& view);
  static std::optional<std::vector<mojo::native::SerializedHandlePtr>> handles(
      IPC::MessageView& view);

  static bool Read(IPC::mojom::MessageDataView data, IPC::MessageView* out);
};

}  // namespace mojo

#endif  // IPC_MESSAGE_MOJOM_TRAITS_H_
