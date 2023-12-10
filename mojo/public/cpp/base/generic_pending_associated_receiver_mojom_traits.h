// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_ASSOCIATED_RECEIVER_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_ASSOCIATED_RECEIVER_MOJOM_TRAITS_H_

#include <string_view>

#include "base/component_export.h"
#include "mojo/public/cpp/bindings/generic_pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/generic_pending_associated_receiver.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::GenericPendingAssociatedReceiverDataView,
                 GenericPendingAssociatedReceiver> {
  static bool IsNull(const GenericPendingAssociatedReceiver& receiver);
  static void SetToNull(GenericPendingAssociatedReceiver* receiver);

  static std::string_view interface_name(
      const GenericPendingAssociatedReceiver& receiver) {
    DCHECK(receiver.interface_name().has_value());
    return receiver.interface_name().value();
  }

  static mojo::ScopedInterfaceEndpointHandle receiver(
      GenericPendingAssociatedReceiver& receiver) {
    return receiver.PassHandle();
  }

  static bool Read(
      mojo_base::mojom::GenericPendingAssociatedReceiverDataView data,
      GenericPendingAssociatedReceiver* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_GENERIC_PENDING_ASSOCIATED_RECEIVER_MOJOM_TRAITS_H_
