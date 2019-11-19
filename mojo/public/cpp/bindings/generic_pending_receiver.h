// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// GenericPendingReceiver encapsulates a pairing of a receiving pipe endpoint
// with the name of the mojom interface assumed by the corresponding remote
// endpoint.
//
// This is used by mojom C++ bindings to represent
// |mojo_base.mojom.GenericPendingReceiver|, and it serves as a semi-safe
// wrapper for transporting arbitrary interface receivers in a generic object.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) GenericPendingReceiver {
 public:
  GenericPendingReceiver();
  GenericPendingReceiver(base::StringPiece interface_name,
                         mojo::ScopedMessagePipeHandle receiving_pipe);

  template <typename Interface>
  GenericPendingReceiver(mojo::PendingReceiver<Interface> receiver)
      : GenericPendingReceiver(Interface::Name_, receiver.PassPipe()) {}

  template <typename Interface>
  GenericPendingReceiver(mojo::InterfaceRequest<Interface> request)
      : GenericPendingReceiver(Interface::Name_, request.PassMessagePipe()) {}

  GenericPendingReceiver(GenericPendingReceiver&&);
  ~GenericPendingReceiver();

  GenericPendingReceiver& operator=(GenericPendingReceiver&&);

  bool is_valid() const { return pipe_.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  void reset();

  const base::Optional<std::string>& interface_name() const {
    return interface_name_;
  }

  mojo::MessagePipeHandle pipe() const { return pipe_.get(); }

  // Takes ownership of the receiving pipe, invalidating this
  // GenericPendingReceiver.
  mojo::ScopedMessagePipeHandle PassPipe();

  // Takes ownership of the pipe, strongly typed as an |Interface| receiver, if
  // and only if that interface's name matches the stored interface name.
  template <typename Interface>
  mojo::PendingReceiver<Interface> As() {
    return mojo::PendingReceiver<Interface>(PassPipeIfNameIs(Interface::Name_));
  }

 private:
  mojo::ScopedMessagePipeHandle PassPipeIfNameIs(const char* interface_name);

  base::Optional<std::string> interface_name_;
  mojo::ScopedMessagePipeHandle pipe_;

  DISALLOW_COPY_AND_ASSIGN(GenericPendingReceiver);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_GENERIC_PENDING_RECEIVER_H_
