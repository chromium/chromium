// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
#define IPC_IPC_MESSAGE_ATTACHMENT_SET_H_

#include <stddef.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ipc/ipc_message_support_export.h"

namespace IPC {

class MessageAttachment;

// -----------------------------------------------------------------------------
// A MessageAttachmentSet is an ordered set of MessageAttachment objects
// associated with an IPC message. All attachments are wrapped in a mojo handle
// if necessary and sent over the mojo message pipe.
//
// For ChannelNacl under SFI NaCl, only Type::PLATFORM_FILE is supported. In
// that case, the FD is sent over socket.
// -----------------------------------------------------------------------------
class IPC_MESSAGE_SUPPORT_EXPORT MessageAttachmentSet
    : public base::RefCountedThreadSafe<MessageAttachmentSet> {
 public:
  MessageAttachmentSet();

  MessageAttachmentSet(const MessageAttachmentSet&) = delete;
  MessageAttachmentSet& operator=(const MessageAttachmentSet&) = delete;

  // Return the number of attachments
  unsigned size() const;

  // Return true if no unconsumed descriptors remain
  bool empty() const { return attachments_.empty(); }

  // Returns whether the attachment was successfully added.
  // |index| is an output variable. On success, it contains the index of the
  // newly added attachment.
  bool AddAttachment(scoped_refptr<MessageAttachment> attachment,
                     size_t* index);

  // Similar to the above method, but without output variables.
  bool AddAttachment(scoped_refptr<MessageAttachment> attachment);

  // Take the nth from the beginning of the vector, Code using this /must/
  // access the attachments in order, and must do it at most once.
  //
  // This interface is designed for the deserialising code as it doesn't
  // support close flags.
  //   returns: an attachment, or nullptr on error
  scoped_refptr<MessageAttachment> GetAttachmentAt(unsigned index);

  // Marks all the descriptors as consumed and closes those which are
  // auto-close.
  void CommitAllDescriptors();

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // This is the maximum number of descriptors per message. We need to know this
  // because the control message kernel interface has to be given a buffer which
  // is large enough to store all the descriptor numbers. Otherwise the kernel
  // tells us that it truncated the control data and the extra descriptors are
  // lost.
  //
  // In debugging mode, it's a fatal error to try and add more than this number
  // of descriptors to a MessageAttachmentSet.
  static const size_t kMaxDescriptorsPerMessage = 7;
#endif  // BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)

  // ---------------------------------------------------------------------------

 private:
  friend class base::RefCountedThreadSafe<MessageAttachmentSet>;

  ~MessageAttachmentSet();

  // Return the number of file descriptors
  unsigned num_descriptors() const;

  std::vector<scoped_refptr<MessageAttachment>> attachments_;

  // This contains the index of the next descriptor which should be consumed.
  // It's used in a couple of ways. Firstly, at destruction we can check that
  // all the descriptors have been read (with GetNthDescriptor). Secondly, we
  // can check that they are read in order.
  unsigned consumed_descriptor_highwater_;
};

}  // namespace IPC

#endif  // IPC_IPC_MESSAGE_ATTACHMENT_SET_H_
