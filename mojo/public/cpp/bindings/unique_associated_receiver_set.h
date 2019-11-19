// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_ASSOCIATED_RECEIVER_SET_H_
#define MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_ASSOCIATED_RECEIVER_SET_H_

#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_ptr_impl_ref_traits.h"

namespace mojo {

// This class manages a set of associated receiving endpoints where each
// endpoint is bound to a unique implementation of the interface owned by this
// object. That is to say, for every bound AssociatedReceiver<T> in the set,
// there is a dedicated unique_ptr<T> owned by the set and receiving messages.
//
// Each owned implementation of T has its lifetime automatically managed by the
// UniqueAssociatedReceiverSet, destroying an instance whenever its receiver is
// disconnected because the remote endpoint intentionally hung up, crashed, or
// sent malformed message.
template <typename Interface,
          typename ContextType = void,
          typename Deleter = std::default_delete<Interface>>
using UniqueAssociatedReceiverSet = ReceiverSetBase<
    AssociatedReceiver<Interface, UniquePtrImplRefTraits<Interface, Deleter>>,
    ContextType>;

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_UNIQUE_ASSOCIATED_RECEIVER_SET_H_
