// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_H_
#define IPC_IPC_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "ipc/ipc_buildflags.h"
#include "ipc/ipc_message_support_export.h"

namespace mojo {
namespace internal {
struct UnmappedNativeStructSerializerImpl;
}
}  // namespace mojo

namespace IPC {

namespace internal {
class ChannelReader;
}  // namespace internal

//------------------------------------------------------------------------------

struct LogData;
class MessageAttachmentSet;

class IPC_MESSAGE_SUPPORT_EXPORT Message : public base::Pickle {
 public:
  enum PriorityValue {
    PRIORITY_LOW = 1,
    PRIORITY_NORMAL,
    PRIORITY_HIGH
  };

  // Bit values used in the flags field.
  // Upper 24 bits of flags store a reference number, so this enum is limited to
  // 8 bits.
  enum {
    PRIORITY_MASK     = 0x03,  // Low 2 bits of store the priority value.
    SYNC_BIT          = 0x04,
    REPLY_BIT         = 0x08,
    REPLY_ERROR_BIT   = 0x10,
    UNBLOCK_BIT       = 0x20,
    PUMPING_MSGS_BIT  = 0x40,  // Deprecated.
    HAS_SENT_TIME_BIT = 0x80,
  };

  ~Message() override;

  Message();

  // Initialize a message with a user-defined type, priority value, and
  // destination WebView ID.
  Message(int32_t routing_id, uint32_t type, PriorityValue priority);

  // Initializes a message from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this message.  Only const methods
  // should be used on the message when initialized this way.
  Message(const char* data, size_t data_len);

  Message(const Message& other);
  Message& operator=(const Message& other);

  bool IsValid() const { return header_size() == sizeof(Header) && header(); }

  PriorityValue priority() const {
    return static_cast<PriorityValue>(header()->flags & PRIORITY_MASK);
  }

  // True if this is a synchronous message.
  void set_sync() {
    header()->flags |= SYNC_BIT;
  }
  bool is_sync() const {
    return (header()->flags & SYNC_BIT) != 0;
  }

  // Set this on a reply to a synchronous message.
  void set_reply() {
    header()->flags |= REPLY_BIT;
  }

  bool is_reply() const {
    return (header()->flags & REPLY_BIT) != 0;
  }

  // Set this on a reply to a synchronous message to indicate that no receiver
  // was found.
  void set_reply_error() {
    header()->flags |= REPLY_ERROR_BIT;
  }

  bool is_reply_error() const {
    return (header()->flags & REPLY_ERROR_BIT) != 0;
  }

  // Normally when a receiver gets a message and they're blocked on a
  // synchronous message Send, they buffer a message.  Setting this flag causes
  // the receiver to be unblocked and the message to be dispatched immediately.
  void set_unblock(bool unblock) {
    if (unblock) {
      header()->flags |= UNBLOCK_BIT;
    } else {
      header()->flags &= static_cast<uint32_t>(~UNBLOCK_BIT);
    }
  }

  bool should_unblock() const {
    return (header()->flags & UNBLOCK_BIT) != 0;
  }

  void set_dispatch_error() const {
    dispatch_error_ = true;
  }

  bool dispatch_error() const {
    return dispatch_error_;
  }

  uint32_t type() const {
    return header()->type;
  }

  int32_t routing_id() const {
    return header()->routing;
  }

  void set_routing_id(int32_t new_id) {
    header()->routing = new_id;
  }

  uint32_t flags() const {
    return header()->flags;
  }

  // Sets all the given header values. The message should be empty at this
  // call.
  void SetHeaderValues(int32_t routing, uint32_t type, uint32_t flags);

  template<class T, class S, class P>
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,
                       void (T::*func)()) {
    (obj->*func)();
    return true;
  }

  template<class T, class S, class P>
  static bool Dispatch(const Message* msg, T* obj, S* sender, P* parameter,
                       void (T::*func)(P*)) {
    (obj->*func)(parameter);
    return true;
  }

  // Used for async messages with no parameters.
  static void Log(std::string* name, const Message* msg, std::string* l) {
  }

  // The static method FindNext() returns several pieces of information, which
  // are aggregated into an instance of this struct.
  struct IPC_MESSAGE_SUPPORT_EXPORT NextMessageInfo {
    NextMessageInfo();
    ~NextMessageInfo();

    // Total message size. Always valid if |message_found| is true.
    // If |message_found| is false but we could determine message size
    // from the header, this field is non-zero. Otherwise it's zero.
    size_t message_size;
    // Whether an entire message was found in the given memory range.
    bool message_found;
    // Only filled in if |message_found| is true.
    // The start address is passed into FindNext() by the caller, so isn't
    // repeated in this struct. The end address of the pickle should be used to
    // construct a base::Pickle.
    const char* pickle_end;
    // Only filled in if |message_found| is true.
    // The end address of the message should be used to determine the start
    // address of the next message.
    const char* message_end;
  };

  // |info| is an output parameter and must not be nullptr.
  static void FindNext(const char* range_start,
                       const char* range_end,
                       NextMessageInfo* info);

  // WriteAttachment appends |attachment| to the end of the set. It returns
  // false iff the set is full.
  bool WriteAttachment(
      scoped_refptr<base::Pickle::Attachment> attachment) override;
  // ReadAttachment parses an attachment given the parsing state |iter| and
  // writes it to |*attachment|. It returns true on success.
  bool ReadAttachment(
      base::PickleIterator* iter,
      scoped_refptr<base::Pickle::Attachment>* attachment) const override;
  // Returns true if there are any attachment in this message.
  bool HasAttachments() const override;

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  // Adds the outgoing time from Time::Now() at the end of the message and sets
  // a bit to indicate that it's been added.
  void set_sent_time(int64_t time);
  int64_t sent_time() const;

  void set_received_time(int64_t time) const;
  int64_t received_time() const { return received_time_; }
  void set_output_params(const std::string& op) const { output_params_ = op; }
  const std::string& output_params() const { return output_params_; }
  // The following four functions are needed so we can log sync messages with
  // delayed replies.  We stick the log data from the sent message into the
  // reply message, so that when it's sent and we have the output parameters
  // we can log it.  As such, we set a flag on the sent message to not log it.
  void set_sync_log_data(LogData* data) const { log_data_ = data; }
  LogData* sync_log_data() const { return log_data_; }
  void set_dont_log() const { dont_log_ = true; }
  bool dont_log() const { return dont_log_; }
#endif

 protected:
  friend class Channel;
  friend class ChannelMojo;
  friend class ChannelNacl;
  friend class ChannelPosix;
  friend class ChannelWin;
  friend class internal::ChannelReader;
  friend class MessageReplyDeserializer;
  friend class SyncMessage;

  friend struct mojo::internal::UnmappedNativeStructSerializerImpl;

#pragma pack(push, 4)
  struct Header : base::Pickle::Header {
    int32_t routing;  // ID of the view that this message is destined for
    uint32_t type;    // specifies the user-defined message type
    uint32_t flags;   // specifies control flags for the message
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    uint16_t num_fds; // the number of descriptors included with this message
    uint16_t pad;     // explicitly initialize this to appease valgrind
#endif
  };
#pragma pack(pop)

  Header* header() {
    return headerT<Header>();
  }
  const Header* header() const {
    return headerT<Header>();
  }

  void Init();

  // Used internally to support IPC::Listener::OnBadMessageReceived.
  mutable bool dispatch_error_;

  // The set of file descriptors associated with this message.
  scoped_refptr<MessageAttachmentSet> attachment_set_;

  // Ensure that a MessageAttachmentSet is allocated
  void EnsureMessageAttachmentSet();

  MessageAttachmentSet* attachment_set() {
    EnsureMessageAttachmentSet();
    return attachment_set_.get();
  }
  const MessageAttachmentSet* attachment_set() const {
    return attachment_set_.get();
  }

#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  // Used for logging.
  mutable int64_t received_time_;
  mutable std::string output_params_;
  mutable raw_ptr<LogData> log_data_;
  mutable bool dont_log_;
#endif

  FRIEND_TEST_ALL_PREFIXES(IPCMessageTest, FindNext);
  FRIEND_TEST_ALL_PREFIXES(IPCMessageTest, FindNextOverflow);
};

//------------------------------------------------------------------------------

}  // namespace IPC

enum SpecialRoutingIDs {
  // indicates that we don't have a routing ID yet.
  MSG_ROUTING_NONE = -2,

  // indicates a general message not sent to a particular tab.
  MSG_ROUTING_CONTROL = INT32_MAX,
};

#define IPC_REPLY_ID 0xFFFFFFF0  // Special message id for replies
#define IPC_LOGGING_ID 0xFFFFFFF1  // Special message id for logging

#endif  // IPC_IPC_MESSAGE_H_
