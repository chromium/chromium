// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_SYNC_MESSAGE_H_
#define IPC_IPC_SYNC_MESSAGE_H_

#include <stdint.h>

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

#include <memory>
#include <string>

#include "build/build_config.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_support_export.h"

namespace base {
class WaitableEvent;
}

namespace IPC {

class MessageReplyDeserializer;

class IPC_MESSAGE_SUPPORT_EXPORT SyncMessage : public Message {
 public:
  SyncMessage(int32_t routing_id,
              uint32_t type,
              PriorityValue priority,
              MessageReplyDeserializer* deserializer);
  ~SyncMessage() override;

  // Call this to get a deserializer for the output parameters.
  // Note that this can only be called once, and the caller is responsible
  // for deleting the deserializer when they're done.
  MessageReplyDeserializer* GetReplyDeserializer();

  // Returns true if the message is a reply to the given request id.
  static bool IsMessageReplyTo(const Message& msg, int request_id);

  // Given a reply message, returns an iterator to the beginning of the data
  // (i.e. skips over the synchronous specific data).
  static base::PickleIterator GetDataIterator(const Message* msg);

  // Given a synchronous message (or its reply), returns its id.
  static int GetMessageId(const Message& msg);

  // Generates a reply message to the given message.
  static Message* GenerateReply(const Message* msg);

 private:
  struct SyncHeader {
    // unique ID (unique per sender)
    int message_id;
  };

  static bool ReadSyncHeader(const Message& msg, SyncHeader* header);
  static bool WriteSyncHeader(Message* msg, const SyncHeader& header);

  std::unique_ptr<MessageReplyDeserializer> deserializer_;
};

// Used to deserialize parameters from a reply to a synchronous message
class IPC_MESSAGE_SUPPORT_EXPORT MessageReplyDeserializer {
 public:
  virtual ~MessageReplyDeserializer() {}
  bool SerializeOutputParameters(const Message& msg);
 private:
  // Derived classes need to implement this, using the given iterator (which
  // is skipped past the header for synchronous messages).
  virtual bool SerializeOutputParameters(const Message& msg,
                                         base::PickleIterator iter) = 0;
};

// When sending a synchronous message, this structure contains an object
// that knows how to deserialize the response.
struct PendingSyncMsg {
  PendingSyncMsg(int id, MessageReplyDeserializer* d, base::WaitableEvent* e)
      : id(id), deserializer(d), done_event(e), send_result(false) {}

  int id;
  raw_ptr<MessageReplyDeserializer, DanglingUntriaged> deserializer;
  raw_ptr<base::WaitableEvent, DanglingUntriaged> done_event;
  bool send_result;
};

}  // namespace IPC

#endif  // IPC_IPC_SYNC_MESSAGE_H_
