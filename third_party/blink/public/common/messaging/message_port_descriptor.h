// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_MESSAGE_PORT_DESCRIPTOR_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_MESSAGE_PORT_DESCRIPTOR_H_

#include "base/unguessable_token.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "third_party/blink/public/common/common_export.h"

namespace blink {

// Defines a message port descriptor, which is a mojo::MessagePipeHandle and
// some associated state which follows the handle around as it is passed from
// one execution context to another. This can be serialized into a
// mojom::MessagePortDescriptor using MessagePortDescriptorStructTraits. Since
// this class uses only POD and Mojo types the same serialization logic is fine
// for use both inside and outside of Blink. This type is move-only so that only
// a single representation of an endpoint can exist at any moment in time.
// This class is not thread-safe, but a MessagePortDescriptor is only ever owned
// by a single thread at a time.
//
// A MessagePortDescriptor should never be created in isolation, but rather they
// should only be created in pairs via MessagePortDescriptorPair.
//
// To enforce that a Mojo pipe isn't left dangling, MessagePortDescriptors
// enforce that they are only destroyed while holding onto their pipe.
//
// This class is intended to be used as follows:
//
//   MessagePortDescriptorPair port_pair;
//   MessagePortDescriptor port0 = port_pair.TakePort0();
//   MessagePortDescriptor port1 = port_pair.TakePort1();
//
//   ... pass around the port descriptors in TransferableMessages as desired ...
//
//   // Pass this into a MessagePortChannel for reference-counted safe-keeping.
//   MessagePortChannel channel0(port0);
//
//   // Entangle into a MessagePort for use in sending messages.
//   MessagePort message_port;
//   message_port.Entangle(channel0)
//   message_port.postMessage(...);
//   channel0 = message_port.Disentangle();
class BLINK_COMMON_EXPORT MessagePortDescriptor {
 public:
  // Delegate used to provide information about the state of message ports.
  // See full class declaration below.
  class InstrumentationDelegate;

  // Allows setting a singleton instrumentation delegate. This is not
  // thread-safe and should be set in early Blink initialization.
  static void SetInstrumentationDelegate(InstrumentationDelegate* delegate);

  MessagePortDescriptor();

  // Disallow copying, and enforce move-only semantics.
  MessagePortDescriptor(const MessagePortDescriptor& message_port) = delete;
  MessagePortDescriptor(MessagePortDescriptor&& message_port);
  MessagePortDescriptor& operator=(const MessagePortDescriptor& message_port) =
      delete;
  MessagePortDescriptor& operator=(MessagePortDescriptor&& message_port);

  ~MessagePortDescriptor();

  // Simple accessors.
  const mojo::ScopedMessagePipeHandle& handle() const { return handle_; }
  const base::UnguessableToken& id() const { return id_; }
  uint64_t sequence_number() const { return sequence_number_; }

  // Returns true if this is a valid descriptor.
  bool IsValid() const;

  // Returns true if this descriptor is currently entangled (meaning that the
  // handle has been vended out via "TakeHandleToEntangle").
  bool IsEntangled() const;

  // Returns true if this is a default initialized descriptor.
  bool IsDefault() const;

  void Reset();

 protected:
  friend class MessagePort;
  friend class MessagePortSerializationAccess;
  friend class MessagePortDescriptorTestHelper;

  // These are only meant to be used for serialization, and as such the values
  // should always be non-default initialized when they are called. Intended for
  // use via MessagePortSerializationAccess. These should only be called for
  // descriptors that actually host non-default values.
  void Init(mojo::ScopedMessagePipeHandle handle,
            base::UnguessableToken id,
            uint64_t sequence_number);
  mojo::ScopedMessagePipeHandle TakeHandle();
  base::UnguessableToken TakeId();
  uint64_t TakeSequenceNumber();

  // Intended for use by MessagePort, for binding/unbinding the handle to/from a
  // mojo::Connector. The handle must be bound directly to a mojo::Connector in
  // order for messages to be sent or received. MessagePort::Entangle is passed
  // a MessagePortDescriptor, and takes the handle from the descriptor in order
  // to bind it to the mojo::Connector. Similarly, MessagePort::Disentangle
  // takes the handle back from the mojo::Connector, gives it back to the
  // MessagePortDescriptor, and releases the MessagePortDescriptor to the
  // caller. See MessagePort::Entangle and MessagePort::Disentangle.
  mojo::ScopedMessagePipeHandle TakeHandleToEntangle(
      const base::UnguessableToken& execution_context_id);
  void GiveDisentangledHandle(mojo::ScopedMessagePipeHandle handle);

 private:
  // For access to NotifyPeer and the following constructor.
  friend class MessagePortDescriptorPair;

  // Creates a new MessagePortDescriptor that wraps the provided brand new
  // handle. A unique id and starting sequence number will be generated. Only
  // meant to be called from MessagePortDescriptorPair.
  explicit MessagePortDescriptor(mojo::ScopedMessagePipeHandle handle);

  // Helper functions for forwarding notifications to the
  // InstrumentationDelegate if it exists.
  void NotifyAttached(const base::UnguessableToken& execution_context_id);
  void NotifyDetached();
  void NotifyDestroyed();

  static constexpr size_t kInvalidSequenceNumber = 0;
  static constexpr size_t kFirstValidSequenceNumber = 1;

  // The handle to the underlying pipe.
  mojo::ScopedMessagePipeHandle handle_;

#if DCHECK_IS_ON()
  // The underlying handle, unmanaged. This is used to enforce that the same
  // handle is given back to this descriptor when it is unentangled. This value
  // is not part of the corresponding Mojo type, as it need not be transferred.
  // It is treated as an extension of |handle_| as far as "TakeHandle*"" is
  // concerned.
  mojo::MessagePipeHandle raw_handle_;
#endif

  // The randomly generated ID of this message handle. The ID follows this
  // MessagePortDescriptor around as it is passed between execution contexts,
  // and allows for bookkeeping across the various contexts. When an execution
  // context entangles itself with a handle, it will report back to the browser
  // the ID of the handle and the execution context as well. This allows for the
  // browser to know who is talking to who.
  base::UnguessableToken id_;

  // The sequence number of the instrumentation message related to this handle.
  // Since these messages can arrive out of order in the browser process, this
  // is used to reorder them so that consistent state can be maintained. This
  // will never be zero for a valid port descriptor.
  uint64_t sequence_number_ = kInvalidSequenceNumber;
};

// Defines a wrapped mojo::MessagePipe, containing 2 MessagePortDescriptors that
// are peers of each other.
class BLINK_COMMON_EXPORT MessagePortDescriptorPair {
 public:
  MessagePortDescriptorPair();
  ~MessagePortDescriptorPair();

  const MessagePortDescriptor& port0() const { return port0_; }
  const MessagePortDescriptor& port1() const { return port1_; }

  MessagePortDescriptor TakePort0() { return std::move(port0_); }
  MessagePortDescriptor TakePort1() { return std::move(port1_); }

 private:
  MessagePortDescriptor port0_;
  MessagePortDescriptor port1_;
};

// A delegate used for instrumenting operations on message handle descriptors.
// These messages allow the observing entity to follow the handle endpoints as
// they travel from one execution context to another, and to know when they are
// bound. If no instrumentation delegate is provided the instrumentation is
// disabled. The implementation needs to be thread-safe.
//
// Note that the sequence numbers associated with each port will never be
// reused, and increment by exactly one for each message received. This allows
// the receiver to order incoming messages and know if a message is missing.
// NotifyMessagePortsCreated is special in that it introduces new ports and
// starts new sequence numbers. The next message received (either a PortAttached
// or PortDestroyed message) will use the subsequent sequence number.
//
// TODO(chrisha): Kill this delegate entirely and move these functions into
// dedicated interfaces, with the MessagePortDescriptor impl directly invoking
// them.
class BLINK_COMMON_EXPORT MessagePortDescriptor::InstrumentationDelegate {
 public:
  virtual ~InstrumentationDelegate() = default;

  // Notifies the instrumentation that a pair of matching ports was created.
  virtual void NotifyMessagePortPairCreated(
      const MessagePortDescriptorPair& pair) = 0;

  // Notifies the instrumentation that a handle has been attached to an
  // execution context.
  virtual void NotifyMessagePortAttached(
      const base::UnguessableToken& port_id,
      uint64_t sequence_number,
      const base::UnguessableToken& execution_context_id) = 0;

  // Notifies the instrumentation that a handle has been detached from an
  // execution context.
  virtual void NotifyMessagePortDetached(const base::UnguessableToken& port_id,
                                         uint64_t sequence_number) = 0;

  // Notifies the instrumentation that a handle has been destroyed.
  virtual void NotifyMessagePortDestroyed(const base::UnguessableToken& port_id,
                                          uint64_t sequence_number) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_MESSAGE_PORT_DESCRIPTOR_H_
