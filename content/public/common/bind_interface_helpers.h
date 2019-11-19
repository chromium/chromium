// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_
#define CONTENT_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_

#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::InterfacePtr<Interface>* ptr) {
  mojo::MessagePipe pipe;
  ptr->Bind(mojo::InterfacePtrInfo<Interface>(std::move(pipe.handle0), 0u));
  host->BindInterface(Interface::Name_, std::move(pipe.handle1));
}
template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::InterfaceRequest<Interface> request) {
  host->BindInterface(Interface::Name_, std::move(request.PassMessagePipe()));
}
template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::PendingRemote<Interface>* remote) {
  auto receiver = remote->InitWithNewPipeAndPassReceiver();
  host->BindInterface(Interface::Name_, receiver.PassPipe());
}
template <typename Host, typename Interface>
void BindInterface(Host* host, mojo::PendingReceiver<Interface> receiver) {
  host->BindInterface(Interface::Name_, receiver.PassPipe());
}

}  // namespace

#endif  // CONTENT_PUBLIC_COMMON_BIND_INTERFACE_HELPERS_H_
