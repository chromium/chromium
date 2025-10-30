// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_H_
#define IPC_IPC_MESSAGE_H_

#include <stddef.h>
#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/pickle.h"
#include "build/build_config.h"
#include "ipc/ipc_message_support_export.h"

namespace mojo {
namespace internal {
struct UnmappedNativeStructSerializerImpl;
}
}  // namespace mojo

namespace IPC {

class MessageAttachmentSet;

// A message is essentially now nothing more than a pickle with an
// attachment set.
class IPC_MESSAGE_SUPPORT_EXPORT Message : public base::Pickle {
 public:
  Message();

  // Initializes a message from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this message.  Only const methods
  // should be used on the message when initialized this way.
  Message(const char* data, size_t data_len);

  Message(const Message& other);
  Message& operator=(const Message& other);

  ~Message() override;

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

 protected:
  friend class Channel;
  friend class MessageReplyDeserializer;
  friend struct mojo::internal::UnmappedNativeStructSerializerImpl;

#pragma pack(push, 4)
  struct Header : base::Pickle::Header {
    int32_t pad_routing = 0;
    uint32_t pad_type = 0;
    uint32_t pad_flags = 0;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    uint16_t num_fds = 0;
    uint16_t pad = 0;
#endif
  };
#pragma pack(pop)

  Header* header() {
    return headerT<Header>();
  }
  const Header* header() const {
    return headerT<Header>();
  }

  // Ensure that a MessageAttachmentSet is allocated
  void EnsureMessageAttachmentSet();

  MessageAttachmentSet* attachment_set() {
    EnsureMessageAttachmentSet();
    return attachment_set_.get();
  }
  const MessageAttachmentSet* attachment_set() const {
    return attachment_set_.get();
  }

  // The set of file descriptors associated with this message.
  scoped_refptr<MessageAttachmentSet> attachment_set_;

  FRIEND_TEST_ALL_PREFIXES(IPCMessageTest, FindNext);
  FRIEND_TEST_ALL_PREFIXES(IPCMessageTest, FindNextOverflow);
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_H_
