// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_
#define MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_

#include <type_traits>
#include <utility>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/disconnect_reason.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/pending_receiver_state.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"
#include "mojo/public/cpp/bindings/runtime_features.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace mojo {

template <typename T>
class PendingRemote;

template <typename T>
struct PendingReceiverConverter;

// A PendingReceiver receives and accumulates a queue of incoming Interface
// method calls made by a single corresponding Remote. PendingReceiver instances
// may be freely moved to another thread/sequence, or even transferred to
// another process via a Mojo interface call (see pending_receiver<T> syntax in
// mojom IDL).
//
// This object should eventually be consumed to bind a Receiver, which will then
// begin dispatching any queued and future incoming method calls to a local
// implementation of Interface. See Receiver documentation for more details.
//
// NOTE: This object is essentially semantic sugar wrapping a message pipe
// handle that is expected to receive Interface messages from a Remote. As such,
// consumers who know what they're doing (i.e. who are confident about what lies
// on the other end of a pipe) may freely convert between a PendingReceiver and
// a raw message pipe handle.
template <typename Interface>
class PendingReceiver {
 public:
  // Constructs an invalid PendingReceiver. This object is not entangled with
  // any Remote and cannot be used to bind a Receiver.
  //
  // A valid PendingReceiver is commonly obtained by calling
  // |Remote::BindNewPipeAndPassReceiver()| on an existing unbound Remote
  // instance or less commonly by calling calling
  // |PendingRemote::InitWithNewPipeAndPassReceiver()| on an existing but
  // invalid PendingRemote instance.
  PendingReceiver() = default;
  PendingReceiver(PendingReceiver&&) noexcept = default;

  // Constructs a valid PendingReceiver from a valid raw message pipe handle.
  explicit PendingReceiver(ScopedMessagePipeHandle pipe)
      : state_(std::move(pipe)) {}

  // Disabled on NaCl since it crashes old version of clang.
#if !BUILDFLAG(IS_NACL)
  // Move conversion operator for custom receiver types. Only participates in
  // overload resolution if a typesafe conversion is supported.
  template <typename T,
            std::enable_if_t<std::is_same<
                PendingReceiver<Interface>,
                std::invoke_result_t<decltype(&PendingReceiverConverter<
                                              T>::template To<Interface>),
                                     T&&>>::value>* = nullptr>
  PendingReceiver(T&& other)
      : PendingReceiver(PendingReceiverConverter<T>::template To<Interface>(
            std::forward<T>(other))) {}
#endif  // !BUILDFLAG(IS_NACL)

  PendingReceiver(const PendingReceiver&) = delete;
  PendingReceiver& operator=(const PendingReceiver&) = delete;

  ~PendingReceiver() = default;

  PendingReceiver& operator=(PendingReceiver&&) noexcept = default;

  // Indicates whether the PendingReceiver is valid, meaning it can be used to
  // bind a Receiver that wants to begin dispatching method calls made by the
  // entangled Remote.
  bool is_valid() const { return state_.pipe.is_valid(); }
  explicit operator bool() const { return is_valid(); }

  // Resets this PendingReceiver to an invalid state. If it was entangled with a
  // Remote or PendingRemote, that object remains in a valid state and will
  // eventually detect that its receiver is gone. Any calls it makes will
  // effectively be dropped.
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

  // Passes ownership of this PendingReceiver's message pipe handle. After this
  // call, the PendingReceiver is no longer in a valid state and can no longer
  // be used to bind a Receiver.
  [[nodiscard]] ScopedMessagePipeHandle PassPipe() {
    return std::move(state_.pipe);
  }

  // Assigns this PendingReceiver to the ConnectionGroup referenced by |ref|.
  // Any Receiver which binds this PendingReceiver will inherit the Ref.
  void set_connection_group(ConnectionGroup::Ref ref) {
    state_.connection_group = std::move(ref);
  }

  const ConnectionGroup::Ref& connection_group() const {
    return state_.connection_group;
  }

  // Passes ownership of this PendingReceiver's ConnectionGroup Ref, removing it
  // from its group.
  ConnectionGroup::Ref PassConnectionGroupRef() {
    return std::move(state_.connection_group);
  }

  // Creates a new message pipe, retaining one end in the PendingReceiver
  // (making it valid) and returning the other end as its entangled
  // PendingRemote. May only be called on an invalid PendingReceiver.
  [[nodiscard]] REINITIALIZES_AFTER_MOVE PendingRemote<Interface>
  InitWithNewPipeAndPassRemote();

  // For internal Mojo use only.
  internal::PendingReceiverState* internal_state() { return &state_; }

 private:
  internal::PendingReceiverState state_;
};

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS) NullReceiver {
 public:
  template <typename Interface>
  operator PendingReceiver<Interface>() const {
    return PendingReceiver<Interface>();
  }
};

}  // namespace mojo

#include "mojo/public/cpp/bindings/pending_remote.h"

namespace mojo {

template <typename Interface>
PendingRemote<Interface>
PendingReceiver<Interface>::InitWithNewPipeAndPassRemote() {
  DCHECK(!is_valid()) << "PendingReceiver already has a remote";
  if (!internal::GetRuntimeFeature_ExpectEnabled<Interface>()) {
    return PendingRemote<Interface>();
  }
  MessagePipe pipe;
  state_.pipe = std::move(pipe.handle0);
  return PendingRemote<Interface>(std::move(pipe.handle1), 0u);
}

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_PENDING_RECEIVER_H_
