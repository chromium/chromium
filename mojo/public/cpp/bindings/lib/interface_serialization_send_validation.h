// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_SEND_VALIDATION_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_SEND_VALIDATION_H_

#include "mojo/public/cpp/bindings/associated_interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_data_view.h"
#include "mojo/public/cpp/bindings/lib/has_send_validation_helper.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace mojo::internal {

// Interface Serialization has no send validation:
template <typename Base, typename T>
struct HasSendValidationSerialize<AssociatedInterfacePtrInfoDataView<Base>,
                                  PendingAssociatedRemote<T>>
    : std::false_type {};

template <typename Base, typename T>
struct HasSendValidationSerialize<AssociatedInterfaceRequestDataView<Base>,
                                  PendingAssociatedReceiver<T>>
    : std::false_type {};

template <typename T>
struct HasSendValidationSerialize<AssociatedInterfaceRequestDataView<T>,
                                  ScopedInterfaceEndpointHandle>
    : std::false_type {};

template <typename Base, typename T>
struct HasSendValidationSerialize<InterfacePtrDataView<Base>, PendingRemote<T>>
    : std::false_type {};

template <typename Base, typename T>
struct HasSendValidationSerialize<InterfaceRequestDataView<Base>,
                                  PendingReceiver<T>> : std::false_type {};
}  // namespace mojo::internal

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_INTERFACE_SERIALIZATION_SEND_VALIDATION_H_
