// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_

#include <cstdint>
#include <utility>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

// A valid PendingRemote is entangled with exactly one Receiver or
// PendingReceiver, and can be consumed to bind a Remote in order to begin
// issuing method calls to that receiver. See Remote documentation for more
// details.
//
// PendingRemote instances may be freely moved to another thread/sequence, or
// even transferred to another process via a Mojo interface call (see
// pending_remote<T> syntax in mojom IDL).
//
// NOTE: This object is essentially semantic sugar wrapping a raw message pipe
// handle that is expected to send Interface messages of a specified version
// (typically 0) to a Receiver. As such, consumers who know what they're doing
// (i.e. who are confident about what lies on the other side of a pipe) may
// freely convert between a PendingRemote and a 2-tuple of
// [raw message pipe handle, expected interface version number].
template <typename Interface>
class PendingRemote {
 public:
  // Constructs an invalid PendingRemote. This object is not entangled with any
  // receiver and cannot be used to bind a Remote.
  //
  // A valid PendingRemote is typically obtained by calling
  // |Receiver::BindNewPipeAndPassRemote()| on an existing unbound Receiver
  // instance.
  //
  // To simultaneously create a valid PendingRemote and an entangled
  // PendingReceiver for rarer cases where both objects need to be passed
  // elsewhere, use the |InitWithNewPipeAndPassReceiver()| method defined below.
  PendingRemote() = default;
  PendingRemote(PendingRemote&&) noexcept = default;

  // Temporary helper for transitioning away from old types. Intentionally an
  // implicit constructor.
  PendingRemote(InterfacePtrInfo<Interface>&& ptr_info)
      : PendingRemote(ptr_info.PassHandle(), ptr_info.version()) {}

  // Constructs a valid PendingRemote over a valid raw message pipe handle and
  // expected interface version number.
  PendingRemote(ScopedMessagePipeHandle pipe, uint32_t version)
      : state_(std::move(pipe), version) {}

  ~PendingRemote() = default;

  PendingRemote& operator=(PendingRemote&&) noexcept = default;

  // Indicates whether the PendingRemote is valid, meaning it can be used to
  // bind a Remote that wants to begin issuing method calls to be dispatched by
  // the entangled Receiver.
  bool is_valid() const { return state_.pipe.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  // Temporary helper for transitioning away from old bindings types. This is
  // intentionally an implicit conversion.
  operator InterfacePtrInfo<Interface>() && {
    return InterfacePtrInfo<Interface>(PassPipe(), version());
  }

  // Resets this PendingRemote to an invalid state. If it was entangled with a
  // Receiver or PendingReceiver, that object remains in a valid state and will
  // eventually detect that its remote caller is gone.
  void reset() { state_.reset(); }

  // Takes ownership of this PendingRemote's message pipe handle. After this
  // call, the PendingRemote is no longer in a valid state and can no longer be
  // used to bind a Remote.
  ScopedMessagePipeHandle PassPipe() WARN_UNUSED_RESULT {
    state_.version = 0;
    return std::move(state_.pipe);
  }
  const ScopedMessagePipeHandle& Pipe() const { return state_.pipe; }

  // The version of the interface this Remote is assuming when making method
  // calls. For the most common case of unversioned mojom interfaces, this is
  // always zero.
  uint32_t version() const { return state_.version; }

  // Creates a new message pipe, retaining one end in the PendingRemote (making
  // it valid) and returning the other end as its entangled PendingReceiver. May
  // only be called on an invalid PendingRemote.
  PendingReceiver<Interface> InitWithNewPipeAndPassReceiver()
      WARN_UNUSED_RESULT {
    DCHECK(!is_valid()) << "PendingRemote already has a receiver";
    MessagePipe pipe;
    state_.pipe = std::move(pipe.handle0);
    return PendingReceiver<Interface>(std::move(pipe.handle1));
  }

  // For internal Mojo use only.
  internal::PendingRemoteState* internal_state() { return &state_; }

 private:
  internal::PendingRemoteState state_;

  DISALLOW_COPY_AND_ASSIGN(PendingRemote);
};

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullRemote {
 public:
  template <typename Interface>
  operator PendingRemote<Interface>() const {
    return PendingRemote<Interface>();
  }
};

// Fuses a PendingReceiver<T> endpoint with a PendingRemote<T> endpoint. The
// endpoints must belong to two different message pipes, and this effectively
// fuses two pipes into a single pipe. Returns |true| on success or |false| on
// failure.
template <typename Interface>
bool FusePipes(PendingReceiver<Interface> receiver,
               PendingRemote<Interface> remote) {
  MojoResult result = FuseMessagePipes(receiver.PassPipe(), remote.PassPipe());
  return result == MOJO_RESULT_OK;
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_
