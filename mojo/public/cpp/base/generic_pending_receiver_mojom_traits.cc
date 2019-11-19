// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/generic_pending_receiver_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<mojo_base::mojom::GenericPendingReceiverDataView,
                  GenericPendingReceiver>::IsNull(const GenericPendingReceiver&
                                                      receiver) {
  return !receiver.is_valid();
}

// static
void StructTraits<mojo_base::mojom::GenericPendingReceiverDataView,
                  GenericPendingReceiver>::SetToNull(GenericPendingReceiver*
                                                         receiver) {
  receiver->reset();
}

// static
bool StructTraits<mojo_base::mojom::GenericPendingReceiverDataView,
                  GenericPendingReceiver>::
    Read(mojo_base::mojom::GenericPendingReceiverDataView data,
         GenericPendingReceiver* out) {
  base::StringPiece interface_name;
  if (!data.ReadInterfaceName(&interface_name))
    return false;
  *out = GenericPendingReceiver(interface_name.as_string(),
                                data.TakeReceivingPipe());
  return true;
}

}  // namespace mojo
