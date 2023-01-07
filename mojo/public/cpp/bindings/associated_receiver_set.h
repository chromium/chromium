// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_SET_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace mojo {

template <typename Interface, typename ImplRefTraits>
struct ReceiverSetTraits<AssociatedReceiver<Interface, ImplRefTraits>> {
  using InterfaceType = Interface;
  using PendingType = PendingAssociatedReceiver<Interface>;
  using ImplPointerType = typename ImplRefTraits::PointerType;
};

template <typename Interface, typename ContextType = void>
using AssociatedReceiverSet =
    ReceiverSetBase<AssociatedReceiver<Interface>, ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ASSOCIATED_RECEIVER_SET_H_
