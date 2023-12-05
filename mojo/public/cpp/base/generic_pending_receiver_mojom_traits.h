// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_RECEIVER_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_RECEIVER_MOJOM_TRAITS_H_

#include <string_view>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/generic_pending_receiver.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::GenericPendingReceiverDataView,
                 GenericPendingReceiver> {
  static bool IsNull(const GenericPendingReceiver& receiver);
  static void SetToNull(GenericPendingReceiver* receiver);

  static std::string_view interface_name(
      const GenericPendingReceiver& receiver) {
    DCHECK(receiver.interface_name().has_value());
    return receiver.interface_name().value();
  }

  static mojo::ScopedMessagePipeHandle receiving_pipe(
      GenericPendingReceiver& receiver) {
    return receiver.PassPipe();
  }

  static bool Read(mojo_base::mojom::GenericPendingReceiverDataView data,
                   GenericPendingReceiver* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_RECEIVER_MOJOM_TRAITS_H_
