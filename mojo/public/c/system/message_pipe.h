// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains types/constants and functions specific to message pipes.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_
#define MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_

#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Used to refer to message objects created by |MojoCreateMessage()|.
typedef uintptr_t MojoMessageHandle;

#define MOJO_MESSAGE_HANDLE_INVALID ((MojoMessageHandle)0)

// Flags passed to |MojoCreateMessagePipe()| via |MojoCreateMessagePipeOptions|.
// See values defined below.
typedef uint32_t MojoCreateMessagePipeFlags;

// No flags. Default behavior.
#define MOJO_CREATE_MESSAGE_PIPE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoCreateMessagePipe()|.
struct MOJO_ALIGNAS(8) MojoCreateMessagePipeOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoCreateMessagePipeFlags|.
  MojoCreateMessagePipeFlags flags;
};
MOJO_STATIC_ASSERT(MOJO_ALIGNOF(int64_t) <= 8, "int64_t has weird alignment");
MOJO_STATIC_ASSERT(sizeof(struct MojoCreateMessagePipeOptions) == 8,
                   "MojoCreateMessagePipeOptions has wrong size");

// Flags passed to |MojoWriteMessage()| via |MojoWriteMessageOptions|. See
// values defined below.
typedef uint32_t MojoWriteMessageFlags;

// No flags. Default behavior.
#define MOJO_WRITE_MESSAGE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoWriteMessage()|.
struct MOJO_ALIGNAS(8) MojoWriteMessageOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoWriteMessageFlags|.
  MojoWriteMessageFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoWriteMessageOptions) == 8,
                   "MojoWriteMessageOptions has wrong size");

// Flags passed to |MojoReadMessage()| via |MojoReadMessageOptions|. See values
// defined below.
typedef uint32_t MojoReadMessageFlags;

// No flags. Default behavior.
#define MOJO_READ_MESSAGE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoReadMessage()|.
struct MOJO_ALIGNAS(8) MojoReadMessageOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoReadMessageFlags|.
  MojoReadMessageFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoReadMessageOptions) == 8,
                   "MojoReadMessageOptions has wrong size");

// Flags passed to |MojoFuseMessagePipes()| via |MojoFuseMessagePipeOptions|.
// See values defined below.
typedef uint32_t MojoFuseMessagePipesFlags;

// No flags. Default behavior.
#define MOJO_FUSE_MESSAGE_PIPES_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoFuseMessagePipes()|.
struct MOJO_ALIGNAS(8) MojoFuseMessagePipesOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoFuseMessagePipesFlags|.
  MojoFuseMessagePipesFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoFuseMessagePipesOptions) == 8,
                   "MojoFuseMessagePipesOptions has wrong size");

// Flags passed to |MojoCreateMessage()| via |MojoCreateMessageOptions|.
typedef uint32_t MojoCreateMessageFlags;

// No flags. Default behavior.
#define MOJO_CREATE_MESSAGE_FLAG_NONE ((uint32_t)0)

// Do not enforce size restrictions on this message, allowing its serialized
// payload to grow arbitrarily large. If this flag is NOT specified, Mojo will
// throw an assertion failure at serialization time when the message exceeds a
// globally configured maximum size.
#define MOJO_CREATE_MESSAGE_FLAG_UNLIMITED_SIZE ((uint32_t)1)

// Options passed to |MojoCreateMessage()|.
struct MOJO_ALIGNAS(8) MojoCreateMessageOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoCreateMessageFlags|.
  MojoCreateMessageFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoCreateMessageOptions) == 8,
                   "MojoCreateMessageOptions has wrong size");

// Flags passed to |MojoSerializeMessage()| via |MojoSerializeMessageOptions|.
typedef uint32_t MojoSerializeMessageFlags;

// No flags. Default behavior.
#define MOJO_SERIALIZE_MESSAGE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoSerializeMessage()|.
struct MOJO_ALIGNAS(8) MojoSerializeMessageOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoSerializeMessageFlags|.
  MojoSerializeMessageFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoSerializeMessageOptions) == 8,
                   "MojoSerializeMessageOptions has wrong size");

// Flags passed to |MojoAppendMessageData()| via |MojoAppendMessageDataOptions|.
typedef uint32_t MojoAppendMessageDataFlags;

// No flags. Default behavior.
#define MOJO_APPEND_MESSAGE_DATA_FLAG_NONE ((uint32_t)0)

// If set, this comments the resulting (post-append) message size as the final
// size of the message payload, in terms of both bytes and attached handles.
#define MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE \
  ((MojoAppendMessageDataFlags)1)

// Options passed to |MojoAppendMessageData()|.
struct MOJO_ALIGNAS(8) MojoAppendMessageDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoAppendMessageDataFlags|.
  MojoAppendMessageDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoAppendMessageDataOptions) == 8,
                   "MojoAppendMessageDataOptions has wrong size");

// Flags passed to |MojoGetMessageData()| via |MojoGetMessageDataOptions|.
typedef uint32_t MojoGetMessageDataFlags;

// No flags. Default behavior.
#define MOJO_GET_MESSAGE_DATA_FLAG_NONE ((uint32_t)0)

// Ignores attached handles when retrieving message data. This leaves any
// attached handles intact and owned by the message object.
#define MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES ((uint32_t)1)

// Options passed to |MojoGetMessageData()|.
struct MOJO_ALIGNAS(8) MojoGetMessageDataOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoGetMessageDataFlags|.
  MojoGetMessageDataFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoGetMessageDataOptions) == 8,
                   "MojoGetMessageDataOptions has wrong size");

// Flags passed to |MojoSetMessageContext()| via |MojoSetMessageContextOptions|.
typedef uint32_t MojoSetMessageContextFlags;

// No flags. Default behavior.
#define MOJO_SET_MESSAGE_CONTEXT_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoSetMessageContext()|.
struct MOJO_ALIGNAS(8) MojoSetMessageContextOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoSetMessageContextFlags|.
  MojoSetMessageContextFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoSetMessageContextOptions) == 8,
                   "MojoSetMessageContextOptions has wrong size");

// Flags passed to |MojoGetMessageContext()| via |MojoGetMessageContextOptions|.
typedef uint32_t MojoGetMessageContextFlags;

// No flags. Default behavior.
#define MOJO_GET_MESSAGE_CONTEXT_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoGetMessageContext()|.
struct MOJO_ALIGNAS(8) MojoGetMessageContextOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoGetMessageContextFlags|.
  MojoGetMessageContextFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoGetMessageContextOptions) == 8,
                   "MojoGetMessageContextOptions has wrong size");

// Flags passed to |MojoNotifyBadMessage()| via |MojoNotifyBadMessageOptions|.
typedef uint32_t MojoNotifyBadMessageFlags;

// No flags. Default behavior.
#define MOJO_NOTIFY_BAD_MESSAGE_FLAG_NONE ((uint32_t)0)

// Options passed to |MojoNotifyBadMessage()|.
struct MOJO_ALIGNAS(8) MojoNotifyBadMessageOptions {
  // The size of this structure, used for versioning.
  uint32_t struct_size;

  // See |MojoNotifyBadMessageFlags|.
  MojoNotifyBadMessageFlags flags;
};
MOJO_STATIC_ASSERT(sizeof(struct MojoNotifyBadMessageOptions) == 8,
                   "MojoNotifyBadMessageOptions has wrong size");

#ifdef __cplusplus
extern "C" {
#endif

// A callback which can serialize a message given some context. Passed to
// |MojoSetMessageContext()| along with a context it knows how to serialize.
// See |MojoSetMessageContext()| for more details.
//
//   |message| is a message object which had |context| attached.
//   |context| the context which was attached to |message|.
//
// Note that the context is always detached from the message immediately before
// this callback is invoked, and that the associated destructor (if any) is also
// invoked on |context| immediately after the serializer returns.
typedef void (*MojoMessageContextSerializer)(MojoMessageHandle message,
                                             uintptr_t context);

// A callback which can be used to destroy a message context after serialization
// or in the event that the message to which it's attached is destroyed without
// ever being serialized.
typedef void (*MojoMessageContextDestructor)(uintptr_t context);

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Creates a message pipe, which is a bidirectional communication channel for
// framed data (i.e., messages). Messages can contain plain data and/or Mojo
// handles.
//
// |options| may be set to null for a message pipe with the default options.
//
// On success, |*message_pipe_handle0| and |*message_pipe_handle1| are set to
// handles for the two endpoints (ports) for the message pipe.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid (e.g.,
//       |*options| is invalid).
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if a process/system/quota/etc. limit has
//       been reached.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateMessagePipe(
    const struct MojoCreateMessagePipeOptions* options,  // Optional.
    MojoHandle* message_pipe_handle0,                    // Out.
    MojoHandle* message_pipe_handle1);                   // Out.

// Writes a message to the message pipe endpoint given by |message_pipe_handle|.
//
// Note that regardless of success or failure, |message| is destroyed by this
// call and therefore invalidated.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., the message was enqueued).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message_pipe_handle| or |message| is
//       invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed.
//       Note that closing an endpoint is not necessarily synchronous (e.g.,
//       across processes), so this function may succeed even if the other
//       endpoint has been closed (in which case the message would be dropped).
//   |MOJO_RESULT_NOT_FOUND| if |message| has neither a context nor serialized
//       buffer attached and therefore has nothing to be written.
MOJO_SYSTEM_EXPORT MojoResult
MojoWriteMessage(MojoHandle message_pipe_handle,
                 MojoMessageHandle message,
                 const struct MojoWriteMessageOptions* options);

// Reads the next message from a message pipe and returns a message as an opaque
// message handle. The returned message must eventually be destroyed using
// |MojoDestroyMessage()|.
//
// Message payload and handles can be accessed using |MojoGetMessageData()|. For
// Unserialized messages, context may be accessed using
// |MojoGetMessageContext()|.
//
// |options| may be null. |message| must be non-null.
//
// Returns:
//   |MOJO_RESULT_OK| on success (i.e., a message was actually read).
//   |MOJO_RESULT_INVALID_ARGUMENT| if some argument was invalid.
//   |MOJO_RESULT_FAILED_PRECONDITION| if the other endpoint has been closed
//       and there are no more messages to read.
//   |MOJO_RESULT_SHOULD_WAIT| if no message was available to be read.
MOJO_SYSTEM_EXPORT MojoResult
MojoReadMessage(MojoHandle message_pipe_handle,
                const struct MojoReadMessageOptions* options,
                MojoMessageHandle* message);

// Fuses two message pipe endpoints together. Given two pipes:
//
//     A <-> B    and    C <-> D
//
// Fusing handle B and handle C results in a single pipe:
//
//     A <-> D
//
// Handles B and C are ALWAYS closed. Any unread messages at C will eventually
// be delivered to A, and any unread messages at B will eventually be delivered
// to D.
//
// NOTE: A handle may only be fused if it is an open message pipe handle which
// has not been written to.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_FAILED_PRECONDITION| if both handles were valid message pipe
//       handles but could not be merged (e.g. one of them has been written to).
//   |MOJO_INVALID_ARGUMENT| if either handle is not a fusable message pipe
//       handle.
MOJO_SYSTEM_EXPORT MojoResult
MojoFuseMessagePipes(MojoHandle handle0,
                     MojoHandle handle1,
                     const struct MojoFuseMessagePipesOptions* options);

// Creates a new message object which may be sent over a message pipe via
// |MojoWriteMessage()|. Returns a handle to the new message object in
// |*message|.
//
// In its initial state the message object cannot be successfully written to a
// message pipe, but must first have either an opaque context or some serialized
// data attached (see |MojoSetMessageContext()| and
// |MojoAppendMessageData()|).
//
// NOTE: Unlike other types of Mojo API objects, messages are NOT thread-safe
// and thus callers of message-related APIs must be careful to restrict usage of
// any given |MojoMessageHandle| to a single thread at a time.
//
// Returns:
//   |MOJO_RESULT_OK| if a new message was created. |*message| contains a handle
//       to the new message object upon return.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is null.
MOJO_SYSTEM_EXPORT MojoResult
MojoCreateMessage(const struct MojoCreateMessageOptions* options,
                  MojoMessageHandle* message);

// Destroys a message object created by |MojoCreateMessage()| or
// |MojoReadMessage()|.
//
// |message|: The message to destroy. Note that if a message has been written
//     to a message pipe using |MojoWriteMessage()|, it is neither necessary nor
//     valid to attempt to destroy it.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was valid and has been freed.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| was not a valid message.
MOJO_SYSTEM_EXPORT MojoResult MojoDestroyMessage(MojoMessageHandle message);

// Forces a message to be serialized in-place if not already serialized.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| was not serialized and is now serialized.
//       In this case its thunks were invoked to perform serialization and
//       ultimately destroy its associated context. The message may still be
//       written to a pipe or decomposed by |MojoGetMessageData()|.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| was already serialized.
//   |MOJO_RESULT_NOT_FOUND| if |message| cannot be serialized (i.e. it was
//       created with null |MojoMessageContextSerializer|.)
//   |MOJO_RESULT_BUSY| if one or more handles provided by the user context
//       reported itself as busy during the serialization attempt. In this case
//       all serialized handles are closed automatically.
//   |MOJO_RESULT_ABORTED| if some other unspecified error occurred during
//       handle serialization. In this case all serialized handles are closed
//       automatically.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message handle.
//
// Note that unserialized messages may be successfully transferred from one
// message pipe endpoint to another without ever being serialized. This function
// allows callers to coerce eager serialization.
MOJO_SYSTEM_EXPORT MojoResult
MojoSerializeMessage(MojoMessageHandle message,
                     const struct MojoSerializeMessageOptions* options);

// Reserves capacity within a message object.
//
// `message`: The message.
// `payload_buffer_size`: The number of bytes to preallocate for the payload
//     of the message.
//
// NOTE: Can only be called on a newly-created message, before serialization
// begins.
//
// If this call succeeds, if `buffer_size` was non-null then `*buffer_size`
// will contain the storage capacity.
//
// Returns:
//   `MOJO_RESULT_OK` upon success. The message's payload capacity is stored
//       into `*buffer_size`.
//   `MOJO_RESULT_INVALID_ARGUMENT` if `message` is not a valid message object.
//   `MOJO_RESULT_FAILED_PRECONDITION` if `message` has a context attached,
//       or if the `message` is already partly or fully serialized.
MOJO_SYSTEM_EXPORT MojoResult
MojoReserveMessageCapacity(MojoMessageHandle message,
                           uint32_t payload_buffer_size,
                           uint32_t* buffer_size);

// Appends data to a message object.
//
// |message|: The message.
// |additional_payload_size|: The number of bytes by which to extend the payload
//     of the message.
// |handles|: Handles to be appended to the message. May be null iff
//     |num_handles| is 0.
// |num_handles|: The number of handles to be appended to the message.
//
// |options| may be null.
//
// If this call succeeds, |*buffer| will contain the address of the data's
// storage if |buffer| was non-null, and if |buffer_size| was non-null then
// |*buffer_size| will contain the storage capacity. The caller may write
// message contents here.
//
// Note that while the size of the returned buffer may exceed the total
// requested size accumulated over one or more calls to
// |MojoAppendMessageData()|, only the extent of caller's requested capacity is
// considered to be part of the message.
//
// A message with attached data must have its capacity finalized before it can
// be transmitted by calling this function with
// |MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE| set in |options->flags|. Note
// that even after this happens, the returned |*buffer| remains valid and
// writable until the message is passed to either |MojoWriteMessage()| or
// |MojoDestroyMessage()|.
//
// Ownership of all handles in |handles| is transferred to the message object if
// and ONLY if this operation succeeds and returns |MOJO_RESULT_OK|. Otherwise
// the caller retains ownership.
//
// Returns:
//   |MOJO_RESULT_OK| upon success. The message's data buffer and size are
//       stored to |*buffer| and |*buffer_size| respectively. Any previously
//       appended data remains intact but may be moved in the event that
//       |*buffer| itself is moved. If
//       |MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE| was set in
//       |options->flags|, the message is ready for transmission.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object;
//       if |num_handles| is non-zero but |handles| is null; or if any handle in
//       |handles| is invalid.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if |additional_payload_size| or
//       |num_handles| exceeds some implementation- or embedder-defined maximum.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| has a context attached.
//   |MOJO_RESULT_BUSY| if one or more handles in |handles| is currently busy
//       and unable to be serialized.
MOJO_SYSTEM_EXPORT MojoResult
MojoAppendMessageData(MojoMessageHandle message,
                      uint32_t payload_size,
                      const MojoHandle* handles,
                      uint32_t num_handles,
                      const struct MojoAppendMessageDataOptions* options,
                      void** buffer,
                      uint32_t* buffer_size);

// Retrieves data attached to a message object.
//
// |message|: The message.
// |num_bytes|: An output parameter which will receive the total size in bytes
//     of the message's payload.
// |buffer|: An output parameter which will receive the address of a buffer
//     containing exactly |*num_bytes| bytes of payload data. This buffer
//     address is not owned by the caller and is only valid as long as the
//     message handle in |message| is valid.
// |num_handles|: An input/output parameter. On input, if not null, this points
//     to value specifying the available capacity (in number of handles) of
//     |handles|. On output, if not null, this will point to a value specifying
//     the actual number of handles available in the serialized message.
// |handles|: A buffer to contain up to (input) |*num_handles| handles. May be
//     null if |num_handles| is null or |*num_handles| is 0.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| is a message with data attached and the
//       provided handle storage is sufficient to contain all handles attached
//       to the message. If |MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES| was set
//       in |options->flags|, any attached handles in the message are left
//       intact and both |handles| and |num_handles| are ignored. Otherwise
//       ownership of any attached handles is transferred to the caller, and
//       |MojoGetMessageData()| may no longer be called on |message| unless
//       |MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES| is used.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |num_handles| is non-null and
//       |*num_handles| is non-zero, but |handles| is null; or if |message| is
//       not a valid message handle.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| is not a fully serialized
//       message. The caller may use |MojoSerializeMessage()| and try again,
//       or |MojoAppendMessageData()| with the
//       |MOJO_APPEND_MESSAGE_DATA_FLAG_COMMIT_SIZE| flag set to complete a
//       partially serialized |message|.
//   |MOJO_RESULT_NOT_FOUND| if the message's handles (if any) contents have
//       already  been extracted (or have failed to be extracted) by a previous
//       call to |MojoGetMessageData()|.
//   |MOJO_RESULT_RESOURCE_EXHAUSTED| if |num_handles| is null and there are
//       handles attached to the message, or if |*num_handles| on input is less
//       than the number of handles attached to the message. Also may be
//       returned if |num_bytes| or |buffer| is null and the message has a non-
//       empty payload. Note that if |MOJO_GET_MESSAGE_DATA_FLAG_IGNORE_HANDLES|
//       is set in |options->flags| any current or previously attached handles
//       are ignored.
//   |MOJO_RESULT_ARBORTED| if the message is in an invalid state and its data
//       and/or handles are unrecoverable. The message is left in this state but
//       future calls to this API will yield the same result.
MOJO_SYSTEM_EXPORT MojoResult
MojoGetMessageData(MojoMessageHandle message,
                   const struct MojoGetMessageDataOptions* options,
                   void** buffer,
                   uint32_t* num_bytes,
                   MojoHandle* handles,
                   uint32_t* num_handles);

// Sets an opaque context value on a message object. The presence of an opaque
// context is mutually exclusive to the presence of message data.
//
// |context| is the context value to associate with this message, and
// |serializer| is a function which may be called at some later time to convert
// |message| to a serialized message object using |context| as input. See
// |MojoMessageContextSerializer| for more details. |destructor| is a function
// which may be called to allow the user to cleanup state associated with
// |context| after serialization or in the event that the message is destroyed
// without ever being serialized.
//
// Typically a caller will use |context| as an opaque pointer to some heap
// object which is effectively owned by the message once this returns. In this
// way, messages can be sent over a message pipe to a peer endpoint in the same
// process as the sender without performing a serialization step.
//
// If the message does need to cross a process boundary or is otherwise
// forced to serialize (see |MojoSerializeMessage()| below), it will be
// serialized by invoking |serializer|.
//
// If |serializer| is null, the created message cannot be serialized. Subsequent
// calls to |MojoSerializeMessage()| on the created message, or any attempt to
// transmit the message across a process boundary, will fail.
//
// If |destructor| is null, it is assumed that no cleanup is required after
// serializing or destroying a message with |context| attached.
//
// A |context| value of zero is invalid, and setting this on a message
// effectively removes its context. This is necessary if the caller wishes to
// attach data to the message or if the caller wants to destroy the message
// object without triggering |destructor|.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| if the opaque context value was successfully set.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object;
//       |options| is non-null and |*options| contains one or more malformed
//       fields; or |context| is zero but either |serializer| or |destructor| is
//       non-null.
//   |MOJO_RESULT_ALREADY_EXISTS| if |message| already has a non-zero context.
//   |MOJO_RESULT_FAILED_PRECONDITION| if |message| has data attached.
MOJO_SYSTEM_EXPORT MojoResult
MojoSetMessageContext(MojoMessageHandle message,
                      uintptr_t context,
                      MojoMessageContextSerializer serializer,
                      MojoMessageContextDestructor destructor,
                      const struct MojoSetMessageContextOptions* options);

// Retrieves a message object's opaque context value.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| if |message| is a valid message object. |*context|
//       contains its opaque context value, which may be zero.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message object
//       or |options| is non-null and |*options| contains one or more malformed
//       fields.
MOJO_SYSTEM_EXPORT MojoResult
MojoGetMessageContext(MojoMessageHandle message,
                      const struct MojoGetMessageContextOptions* options,
                      uintptr_t* context);

// Notifies the system that a bad message was received on a message pipe,
// according to whatever criteria the caller chooses. This ultimately tries to
// notify the embedder about the bad message, and the embedder may enforce some
// policy for dealing with the source of the message (e.g. close the pipe,
// terminate, a process, etc.) The embedder may not be notified if the calling
// process has lost its connection to the source process.
//
// |message|: The message to report as bad.
// |error|: An error string which may provide the embedder with context when
//     notified of this error.
// |error_num_bytes|: The length of |error| in bytes.
//
// |options| may be null.
//
// Returns:
//   |MOJO_RESULT_OK| if successful.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |message| is not a valid message.
MOJO_SYSTEM_EXPORT MojoResult
MojoNotifyBadMessage(MojoMessageHandle message,
                     const char* error,
                     uint32_t error_num_bytes,
                     const struct MojoNotifyBadMessageOptions* options);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_MESSAGE_PIPE_H_
