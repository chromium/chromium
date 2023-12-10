// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_

#include <cstdint>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/pending_remote_state.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

template <typename T>
class PendingReceiver;

template <typename T>
struct PendingRemoteConverter;

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

  // Constructs a valid PendingRemote over a valid raw message pipe handle and
  // expected interface version number.
  PendingRemote(ScopedMessagePipeHandle pipe, uint32_t version)
      : state_(std::move(pipe), version) {}

  // Disabled on NaCl since it crashes old version of clang.
#if !BUILDFLAG(IS_NACL)
  // Move conversion operator for custom remote types. Only participates in
  // overload resolution if a typesafe conversion is supported.
  template <typename T,
            std::enable_if_t<std::is_same<
                PendingRemote<Interface>,
                std::invoke_result_t<decltype(&PendingRemoteConverter<
                                              T>::template To<Interface>),
                                     T&&>>::value>* = nullptr>
  PendingRemote(T&& other)
      : PendingRemote(PendingRemoteConverter<T>::template To<Interface>(
            std::move(other))) {}
#endif  // !BUILDFLAG(IS_NACL)

  PendingRemote(const PendingRemote&) = delete;
  PendingRemote& operator=(const PendingRemote&) = delete;

  ~PendingRemote() = default;

  PendingRemote& operator=(PendingRemote&&) noexcept = default;

  // Indicates whether the PendingRemote is valid, meaning it can be used to
  // bind a Remote that wants to begin issuing method calls to be dispatched by
  // the entangled Receiver.
  bool is_valid() const { return state_.pipe.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  // Resets this PendingRemote to an invalid state. If it was entangled with a
  // Receiver or PendingReceiver, that object remains in a valid state and will
  // eventually detect that its remote caller is gone.
  void reset() { state_.reset(); }

  // Like above but provides a reason for the disconnection.
  void ResetWithReason(uint32_t reason, const std::string& description) {
    CHECK(is_valid()) << "Cannot send reset reason to an invalid handle.";

    Message message =
        PipeControlMessageProxy::ConstructPeerEndpointClosedMessage(
            kPrimaryInterfaceId, DisconnectReason(reason, description));
    MojoResult result =
        WriteMessageNew(state_.pipe.get(), message.TakeMojoMessage(),
                        MOJO_WRITE_MESSAGE_FLAG_NONE);
    DCHECK_EQ(MOJO_RESULT_OK, result);

    reset();
  }

  // Takes ownership of this PendingRemote's message pipe handle. After this
  // call, the PendingRemote is no longer in a valid state and can no longer be
  // used to bind a Remote.
  [[nodiscard]] ScopedMessagePipeHandle PassPipe() {
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
  [[nodiscard]] REINITIALIZES_AFTER_MOVE PendingReceiver<Interface>
  InitWithNewPipeAndPassReceiver();

  // For internal Mojo use only.
  internal::PendingRemoteState* internal_state() { return &state_; }

 private:
  internal::PendingRemoteState state_;
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

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace mojo {

template <typename Interface>
PendingReceiver<Interface>
PendingRemote<Interface>::InitWithNewPipeAndPassReceiver() {
  DCHECK(!is_valid()) << "PendingReceiver already has a remote";
  if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
    return PendingReceiver<Interface>();
  }
  MessagePipe pipe;
  state_.pipe = std::move(pipe.handle0);
  return PendingReceiver<Interface>(std::move(pipe.handle1));
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_REMOTE_H_
