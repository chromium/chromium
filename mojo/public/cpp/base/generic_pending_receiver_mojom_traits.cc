// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/generic_pending_receiver_mojom_traits.h"

#include "base/strings/string_piece.h"

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
  *out = GenericPendingReceiver(interface_name, data.TakeReceivingPipe());
  return true;
}

}  // namespace mojo
