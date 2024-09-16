// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_
#define MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/connection_group.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/unserialized_message_context.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "mojo/public/cpp/system/message.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace mojo {

class AssociatedGroupController;

using ReportBadMessageCallback =
    base::OnceCallback<void(std::string_view error)>;

COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
BASE_DECLARE_FEATURE(kMojoMessageAlwaysUseLatestVersion);

// Message is a holder for the data and handles to be sent over a MessagePipe.
// Message owns its data and handles, but a consumer of Message is free to
// mutate the data and handles. The message's data is comprised of a header
// followed by payload.
class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) Message {
 public:
  static const uint32_t kFlagExpectsResponse = 1 << 0;
  static const uint32_t kFlagIsResponse = 1 << 1;
  static const uint32_t kFlagIsSync = 1 << 2;
  static const uint32_t kFlagNoInterrupt = 1 << 3;
  static const uint32_t kFlagIsUrgent = 1 << 4;

  // Constructs an uninitialized Message object.
  Message();

  // See the move-assignment operator below.
  Message(Message&& other);

  // Constructs a new message with an unserialized context attached. This
  // message may be serialized later if necessary.
  Message(std::unique_ptr<internal::UnserializedMessageContext> context,
          MojoCreateMessageFlags create_message_flags);

  // Constructs a new serialized Message object with optional handles attached.
  // This message is fully functional and may be exchanged for a
  // ScopedMessageHandle for transit over a message pipe. See TakeMojoMessage().
  //
  // If |handles| is non-null, any handles in |*handles| are attached to the
  // newly constructed message.
  //
  // Note that |payload_size| is only the initially known size of the message
  // payload, if any. The payload can be expanded after construction using the
  // interface returned by |payload_buffer()|.
  //
  // |estimated_payload_size| will be used to preallocate an appropriate amount
  // of memory for the message buffer, based on the history of previous
  // allocations for this message's |name|.
  Message(uint32_t name,
          uint32_t flags,
          size_t payload_size,
          size_t payload_interface_id_count,
          MojoCreateMessageFlags create_message_flags,
          std::vector<ScopedHandle>* handles,
          size_t estimated_payload_size = 0);

  // Same as above, but the with default MojoCreateMessageFlags.
  Message(uint32_t name,
          uint32_t flags,
          size_t payload_size,
          size_t payload_interface_id_count,
          std::vector<ScopedHandle>* handles,
          size_t estimated_payload_size = 0);

  // Constructor for the common case of unknown `payload_size`, unspecified
  // `payload_interface_id_count`, and no `handles` vector.
  Message(uint32_t name,
          uint32_t flags,
          MojoCreateMessageFlags create_message_flags,
          size_t estimated_payload_size);

  // Same as above, but the with default MojoCreateMessageFlags.
  Message(uint32_t name, uint32_t flags, size_t estimated_payload_size);

  // Constructs a new Message object from an existing message handle. Used
  // exclusively for serializing an existing unserialized message.
  explicit Message(ScopedMessageHandle handle,
                   const internal::MessageHeaderV1& header);

  // Constructs a new serialized Message object from a fully populated message
  // payload (including a well-formed message header) and an optional set of
  // handle attachments. This Message may not be extended with additional
  // payload or handles once constructed, but its payload remains mutable as
  // long as the Message is not moved and neither |Reset()| nor
  // |TakeMojoMessage()| is called.
  Message(base::span<const uint8_t> payload, base::span<ScopedHandle> handles);

  // Constructs a new serialized Message object from an existing
  // ScopedMessageHandle; e.g., one read from a message pipe.
  //
  // If the message had any handles attached, they will be extracted and
  // retrievable via |handles()|. Such messages may NOT be sent back over
  // another message pipe, but are otherwise safe to inspect and pass around.
  //
  // If handles are attached and their extraction fails for any reason,
  // |*handle| remains unchanged and the returned Message will be null (i.e.
  // calling IsNull() on it will return |true|).
  static Message CreateFromMessageHandle(ScopedMessageHandle* message_handle);

  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;

  ~Message();

  // Moves |other| into a new Message object. The moved-from Message becomes
  // invalid and is effectively in a default-constructed state after this call.
  Message& operator=(Message&& other);

  // Resets the Message to an uninitialized state. Upon reset, the Message
  // exists as if it were default-constructed: it has no data buffer and owns no
  // handles.
  void Reset();

  // Indicates whether this Message is uninitialized.
  bool IsNull() const { return !handle_.is_valid(); }

  // Indicates whether this Message is in valid state. A Message may be in an
  // invalid state iff it failed partial deserialization during construction
  // over a ScopedMessageHandle.
  bool IsValid() const;

  // Indicates whether this Message is serialized.
  bool is_serialized() const { return serialized_; }

  // Access the raw bytes of the message.
  const uint8_t* data() const {
    DCHECK(payload_buffer_.is_valid());
    return static_cast<const uint8_t*>(payload_buffer_.data());
  }
  uint8_t* mutable_data() { return const_cast<uint8_t*>(data()); }

  size_t data_num_bytes() const {
    DCHECK(payload_buffer_.is_valid());
    return payload_buffer_.cursor();
  }

  // Access the header.
  const internal::MessageHeader* header() const {
    return reinterpret_cast<const internal::MessageHeader*>(data());
  }
  internal::MessageHeader* header() {
    return reinterpret_cast<internal::MessageHeader*>(mutable_data());
  }

  const internal::MessageHeaderV1* header_v1() const {
    DCHECK_GE(version(), 1u);
    return reinterpret_cast<const internal::MessageHeaderV1*>(data());
  }
  internal::MessageHeaderV1* header_v1() {
    DCHECK_GE(version(), 1u);
    return reinterpret_cast<internal::MessageHeaderV1*>(mutable_data());
  }

  const internal::MessageHeaderV2* header_v2() const {
    DCHECK_GE(version(), 2u);
    return reinterpret_cast<const internal::MessageHeaderV2*>(data());
  }
  internal::MessageHeaderV2* header_v2() {
    DCHECK_GE(version(), 2u);
    return reinterpret_cast<internal::MessageHeaderV2*>(mutable_data());
  }

  const internal::MessageHeaderV3* header_v3() const {
    DCHECK_GE(version(), 3u);
    return reinterpret_cast<const internal::MessageHeaderV3*>(data());
  }
  internal::MessageHeaderV3* header_v3() {
    DCHECK_GE(version(), 3u);
    return reinterpret_cast<internal::MessageHeaderV3*>(mutable_data());
  }

  uint32_t version() const { return header()->version; }

  uint32_t interface_id() const { return header()->interface_id; }
  void set_interface_id(uint32_t id) { header()->interface_id = id; }

  uint32_t name() const { return header()->name; }
  bool has_flag(uint32_t flag) const { return !!(header()->flags & flag); }

  // Access the request_id field (if present).
  uint64_t request_id() const { return header_v1()->request_id; }
  void set_request_id(uint64_t request_id) {
    header_v1()->request_id = request_id;
  }

  void set_trace_nonce(uint32_t trace_nonce) {
    header()->trace_nonce = trace_nonce;
  }

  // Access the payload.
  const uint8_t* payload() const;
  uint8_t* mutable_payload() { return const_cast<uint8_t*>(payload()); }
  uint32_t payload_num_bytes() const;

  uint32_t payload_num_interface_ids() const;
  const uint32_t* payload_interface_ids() const;

  internal::Buffer* payload_buffer() { return &payload_buffer_; }

  // Access the handles of a received message. Note that these are unused on
  // outgoing messages.
  const std::vector<ScopedHandle>* handles() const { return &handles_; }
  std::vector<ScopedHandle>* mutable_handles() { return &handles_; }

  const std::vector<ScopedInterfaceEndpointHandle>*
  associated_endpoint_handles() const {
    return &associated_endpoint_handles_;
  }
  std::vector<ScopedInterfaceEndpointHandle>*
  mutable_associated_endpoint_handles() {
    return &associated_endpoint_handles_;
  }

  // Sets the ConnectionGroup to which this Message's local receiver belongs, if
  // any. This is called immediately after a Message is read from a message pipe
  // but before it's deserialized. If non-null, |ref| must point to a Ref that
  // outlives this Message object.
  void set_receiver_connection_group(const ConnectionGroup::Ref* ref) {
    receiver_connection_group_ = ref;
  }
  const ConnectionGroup::Ref* receiver_connection_group() const {
    return receiver_connection_group_;
  }

  // Takes a scoped MessageHandle which may be passed to |WriteMessageNew()| for
  // transmission. Note that this invalidates this Message object, taking
  // ownership of its internal storage and any attached handles.
  ScopedMessageHandle TakeMojoMessage();

  // Notifies the system that this message is "bad," in this case meaning it was
  // rejected by bindings validation code.
  void NotifyBadMessage(std::string_view error);

  // Serializes and attaches Mojo handles and associated endpoint handles from
  // |handles_| and |associated_endpoint_handles_| respectively.
  void SerializeHandles(AssociatedGroupController* group_controller);

  // Deserializes associated endpoint handles from the payload_interface_ids
  // field, into |associated_endpoint_handles_|.
  bool DeserializeAssociatedEndpointHandles(
      AssociatedGroupController* group_controller);

  // If this message contains serialized associated interface endponits but is
  // going to be destroyed without being sent across a pipe, this notifies any
  // relevant local peer endpoints about peer closure. Must be called on any
  // unsent Message that is going to be destroyed after calling
  // SerializeHandles().
  void NotifyPeerClosureForSerializedHandles(
      AssociatedGroupController* group_controller);

  // If this Message has an unserialized message context attached, force it to
  // be serialized immediately. Otherwise this does nothing.
  void SerializeIfNecessary();

  // Takes the unserialized message context from this Message if its tag matches
  // |tag|.
  std::unique_ptr<internal::UnserializedMessageContext> TakeUnserializedContext(
      uintptr_t tag);

  template <typename MessageType>
  std::unique_ptr<MessageType> TakeUnserializedContext() {
    auto generic_context = TakeUnserializedContext(
        reinterpret_cast<uintptr_t>(&MessageType::kMessageTag));
    if (!generic_context)
      return nullptr;
    return base::WrapUnique(
        generic_context.release()->template SafeCast<MessageType>());
  }

  const char* heap_profiler_tag() const { return heap_profiler_tag_; }
  void set_heap_profiler_tag(const char* heap_profiler_tag) {
    heap_profiler_tag_ = heap_profiler_tag;
  }

  // Get a global trace id identifying this message. Used for connecting the
  // sender and the receiver in traces.
  uint64_t GetTraceId() const;

  // Write a representation of this object into a trace.
  void WriteIntoTrace(perfetto::TracedValue ctx) const;

#if defined(ENABLE_IPC_FUZZER)
  const char* interface_name() const { return interface_name_; }
  void set_interface_name(const char* interface_name) {
    interface_name_ = interface_name;
  }

  const char* method_name() const { return method_name_; }
  void set_method_name(const char* method_name) { method_name_ = method_name; }
#endif

  int64_t creation_timeticks_us() const;

 private:
  // Internal constructor used by |CreateFromMessageHandle()| when either there
  // are no attached handles or all attached handles are successfully extracted
  // from the message object.
  Message(ScopedMessageHandle message_handle,
          std::vector<ScopedHandle> attached_handles,
          internal::Buffer payload_buffer,
          bool serialized);

  ScopedMessageHandle handle_;

  // A Buffer which may be used to allocate blocks of data within the message
  // payload for reading or writing.
  internal::Buffer payload_buffer_;

  std::vector<ScopedHandle> handles_;
  std::vector<ScopedInterfaceEndpointHandle> associated_endpoint_handles_;
  raw_ptr<const ConnectionGroup::Ref, DanglingUntriaged>
      receiver_connection_group_ = nullptr;

  // Indicates whether this Message object is transferable, i.e. can be sent
  // elsewhere. In general this is true unless |handle_| is invalid or
  // serialized handles have been extracted from the serialized message object
  // identified by |handle_|.
  bool transferable_ = false;

  // Indicates whether this Message object is serialized.
  bool serialized_ = false;

  const char* heap_profiler_tag_ = nullptr;
#if defined(ENABLE_IPC_FUZZER)
  const char* interface_name_ = nullptr;
  const char* method_name_ = nullptr;
#endif
};

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageFilter {
 public:
  virtual ~MessageFilter() {}

  // The filter may mutate the given message.  This method is called before
  // the message is dispatched to the associated MessageReceiver. Returns true
  // if the message was accepted and false otherwise, indicating that the
  // message was invalid or malformed.
  [[nodiscard]] virtual bool WillDispatch(Message* message) = 0;

  // The filter receives notification that the message was dispatched or
  // rejected. Since the message filter is owned by the receiver it will not be
  // invoked if the receiver is closed during a dispatch of a message.
  virtual void DidDispatchOrReject(Message* message, bool accepted) = 0;
};

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) MessageReceiver {
 public:
  virtual ~MessageReceiver() {}

  // Indicates whether the receiver prefers to receive serialized messages.
  virtual bool PrefersSerializedMessages();

  // The receiver may mutate the given message.  Returns true if the message
  // was accepted and false otherwise, indicating that the message was invalid
  // or malformed.
  [[nodiscard]] virtual bool Accept(Message* message) = 0;
};

class MessageReceiverWithResponder : public MessageReceiver {
 public:
  ~MessageReceiverWithResponder() override {}

  // A variant on Accept that registers a MessageReceiver (known as the
  // responder) to handle the response message generated from the given
  // message. The responder's Accept method may be called during
  // AcceptWithResponder or some time after its return.
  [[nodiscard]] virtual bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiver> responder) = 0;
};

// A MessageReceiver that is also able to provide status about the state
// of the underlying MessagePipe to which it will be forwarding messages
// received via the |Accept()| call.
class MessageReceiverWithStatus : public MessageReceiver {
 public:
  ~MessageReceiverWithStatus() override {}

  // Returns |true| if this MessageReceiver is currently bound to a MessagePipe,
  // the pipe has not been closed, and the pipe has not encountered an error.
  virtual bool IsConnected() = 0;

  // Determines if this MessageReceiver is still bound to a message pipe and has
  // not encountered any errors. This is asynchronous but may be called from any
  // sequence. |callback| is eventually invoked from an arbitrary sequence with
  // the result of the query.
  virtual void IsConnectedAsync(base::OnceCallback<void(bool)> callback) = 0;
};

// An alternative to MessageReceiverWithResponder for cases in which it
// is necessary for the implementor of this interface to know about the status
// of the MessagePipe which will carry the responses.
class MessageReceiverWithResponderStatus : public MessageReceiver {
 public:
  ~MessageReceiverWithResponderStatus() override {}

  // A variant on Accept that registers a MessageReceiverWithStatus (known as
  // the responder) to handle the response message generated from the given
  // message. Any of the responder's methods (Accept or IsValid) may be called
  // during  AcceptWithResponder or some time after its return.
  [[nodiscard]] virtual bool AcceptWithResponder(
      Message* message,
      std::unique_ptr<MessageReceiverWithStatus> responder) = 0;
};

class COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) PassThroughFilter
    : public MessageReceiver {
 public:
  PassThroughFilter();

  PassThroughFilter(const PassThroughFilter&) = delete;
  PassThroughFilter& operator=(const PassThroughFilter&) = delete;

  ~PassThroughFilter() override;

  // MessageReceiver:
  bool Accept(Message* message) override;
};

// Reports the currently dispatching Message as bad. Note that this is only
// legal to call from directly within the stack frame of a message dispatch. If
// you need to do asynchronous work before you can determine the legitimacy of
// a message, use GetBadMessageCallback() and retain its result until you're
// ready to invoke or discard it.
NOT_TAIL_CALLED COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE) void ReportBadMessage(
    std::string_view error);

// Acquires a callback which may be run to report the currently dispatching
// Message as bad. Note that this is only legal to call from directly within the
// stack frame of a message dispatch, but the returned callback may be called
// exactly once any time thereafter to report the message as bad. This may only
// be called once per message.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
ReportBadMessageCallback GetBadMessageCallback();

// Returns true if called directly within the stack frame of a message dispatch.
// Unlike GetBadMessageCallback(), this can be called multiple times.
COMPONENT_EXPORT(MOJO_CPP_BINDINGS_BASE)
bool IsInMessageDispatch();

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_MESSAGE_H_
