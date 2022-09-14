// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_attachment_set.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "ipc/ipc_message_attachment.h"

namespace IPC {

namespace {

unsigned count_attachments_of_type(
    const std::vector<scoped_refptr<MessageAttachment>>& attachments,
    MessageAttachment::Type type) {
  unsigned count = 0;
  for (const scoped_refptr<MessageAttachment>& attachment : attachments) {
    if (attachment->GetType() == type)
      ++count;
  }
  return count;
}

}  // namespace

MessageAttachmentSet::MessageAttachmentSet()
    : consumed_descriptor_highwater_(0) {
}

MessageAttachmentSet::~MessageAttachmentSet() {
  if (consumed_descriptor_highwater_ == size())
    return;

  // We close all the owning descriptors. If this message should have
  // been transmitted, then closing those with close flags set mirrors
  // the expected behaviour.
  //
  // If this message was received with more descriptors than expected
  // (which could a DOS against the browser by a rogue renderer) then all
  // the descriptors have their close flag set and we free all the extra
  // kernel resources.
  LOG(WARNING) << "MessageAttachmentSet destroyed with unconsumed attachments: "
               << consumed_descriptor_highwater_ << "/" << size();
}

unsigned MessageAttachmentSet::num_descriptors() const {
  return count_attachments_of_type(attachments_,
                                   MessageAttachment::Type::PLATFORM_FILE);
}

unsigned MessageAttachmentSet::size() const {
  return static_cast<unsigned>(attachments_.size());
}

bool MessageAttachmentSet::AddAttachment(
    scoped_refptr<MessageAttachment> attachment,
    size_t* index) {
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  if (attachment->GetType() == MessageAttachment::Type::PLATFORM_FILE &&
      num_descriptors() == kMaxDescriptorsPerMessage) {
    DLOG(WARNING) << "Cannot add file descriptor. MessageAttachmentSet full.";
    return false;
  }
#endif

  switch (attachment->GetType()) {
    case MessageAttachment::Type::PLATFORM_FILE:
    case MessageAttachment::Type::MOJO_HANDLE:
    case MessageAttachment::Type::WIN_HANDLE:
    case MessageAttachment::Type::MACH_PORT:
    case MessageAttachment::Type::FUCHSIA_HANDLE:
      attachments_.push_back(attachment);
      *index = attachments_.size() - 1;
      return true;
  }
  return false;
}

bool MessageAttachmentSet::AddAttachment(
    scoped_refptr<MessageAttachment> attachment) {
  size_t index;
  return AddAttachment(attachment, &index);
}

scoped_refptr<MessageAttachment> MessageAttachmentSet::GetAttachmentAt(
    unsigned index) {
  if (index >= size()) {
    DLOG(WARNING) << "Accessing out of bound index:" << index << "/" << size();
    return scoped_refptr<MessageAttachment>();
  }

  // We should always walk the descriptors in order, so it's reasonable to
  // enforce this. Consider the case where a compromised renderer sends us
  // the following message:
  //
  //   ExampleMsg:
  //     num_fds:2 msg:FD(index = 1) control:SCM_RIGHTS {n, m}
  //
  // Here the renderer sent us a message which should have a descriptor, but
  // actually sent two in an attempt to fill our fd table and kill us. By
  // setting the index of the descriptor in the message to 1 (it should be
  // 0), we would record a highwater of 1 and then consider all the
  // descriptors to have been used.
  //
  // So we can either track of the use of each descriptor in a bitset, or we
  // can enforce that we walk the indexes strictly in order.
  if (index == 0 && consumed_descriptor_highwater_ == size()) {
    DLOG(WARNING) << "Attempted to double-read a message attachment, "
                     "returning a nullptr";
  }

  if (index != consumed_descriptor_highwater_)
    return scoped_refptr<MessageAttachment>();

  consumed_descriptor_highwater_ = index + 1;

  return attachments_[index];
}

void MessageAttachmentSet::CommitAllDescriptors() {
  attachments_.clear();
  consumed_descriptor_highwater_ = 0;
}

}  // namespace IPC
