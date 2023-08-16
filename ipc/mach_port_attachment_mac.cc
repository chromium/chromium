// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mach_port_attachment_mac.h"

#include <stdint.h>

#include "base/apple/mach_logging.h"

namespace IPC {
namespace internal {

MachPortAttachmentMac::MachPortAttachmentMac(mach_port_t mach_port)
    : mach_port_(mach_port), owns_mach_port_(true) {
  if (mach_port != MACH_PORT_NULL) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port,
                                          MACH_PORT_RIGHT_SEND, 1);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
        << "MachPortAttachmentMac mach_port_mod_refs";
  }
}
MachPortAttachmentMac::MachPortAttachmentMac(mach_port_t mach_port,
                                             FromWire from_wire)
    : mach_port_(mach_port), owns_mach_port_(true) {}

MachPortAttachmentMac::~MachPortAttachmentMac() {
  if (mach_port_ != MACH_PORT_NULL && owns_mach_port_) {
    kern_return_t kr = mach_port_mod_refs(mach_task_self(), mach_port_,
                                          MACH_PORT_RIGHT_SEND, -1);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr)
        << "~MachPortAttachmentMac mach_port_mod_refs";
  }
}

MessageAttachment::Type MachPortAttachmentMac::GetType() const {
  return Type::MACH_PORT;
}

}  // namespace internal
}  // namespace IPC
