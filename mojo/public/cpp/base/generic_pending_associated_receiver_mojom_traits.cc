// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/generic_pending_associated_receiver_mojom_traits.h"

#include <string_view>

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::GenericPendingAssociatedReceiverDataView,
                  GenericPendingAssociatedReceiver>::
    IsNull(const GenericPendingAssociatedReceiver& receiver) {
  return !receiver.is_valid();
}

// static
void StructTraits<mojo_base::mojom::GenericPendingAssociatedReceiverDataView,
                  GenericPendingAssociatedReceiver>::
    SetToNull(GenericPendingAssociatedReceiver* receiver) {
  receiver->reset();
}

// static
bool StructTraits<mojo_base::mojom::GenericPendingAssociatedReceiverDataView,
                  GenericPendingAssociatedReceiver>::
    Read(mojo_base::mojom::GenericPendingAssociatedReceiverDataView data,
         GenericPendingAssociatedReceiver* out) {
  std::string_view interface_name;
  if (!data.ReadInterfaceName(&interface_name))
    return false;
  *out = GenericPendingAssociatedReceiver(
      interface_name, data.TakeReceiver<ScopedInterfaceEndpointHandle>());
  return true;
}

}  // namespace mojo
