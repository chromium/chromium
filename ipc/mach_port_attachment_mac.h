// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MACH_PORT_ATTACHMENT_MAC_H_
#define IPC_MACH_PORT_ATTACHMENT_MAC_H_

#include <mach/mach.h>
#include <stdint.h>

#include "base/process/process_handle.h"
#include "ipc/ipc_message_attachment.h"
#include "ipc/ipc_message_support_export.h"
#include "ipc/mach_port_mac.h"

namespace IPC {
namespace internal {

// This class represents an OSX mach_port_t attached to a Chrome IPC message.
class IPC_MESSAGE_SUPPORT_EXPORT MachPortAttachmentMac
    : public MessageAttachment {
 public:
  // This constructor increments the ref count of |mach_port_| and takes
  // ownership of the result. Should only be called by the sender of a Chrome
  // IPC message.
  explicit MachPortAttachmentMac(mach_port_t mach_port);

  MachPortAttachmentMac(const MachPortAttachmentMac&) = delete;
  MachPortAttachmentMac& operator=(const MachPortAttachmentMac&) = delete;

  enum FromWire {
    FROM_WIRE,
  };
  // This constructor takes ownership of |mach_port|, but does not modify its
  // ref count. Should only be called by the receiver of a Chrome IPC message.
  MachPortAttachmentMac(mach_port_t mach_port, FromWire from_wire);

  Type GetType() const override;

  mach_port_t get_mach_port() const { return mach_port_; }

  // The caller of this method has taken ownership of |mach_port_|.
  void reset_mach_port_ownership() { owns_mach_port_ = false; }

 private:
  ~MachPortAttachmentMac() override;
  const mach_port_t mach_port_;

  // In the sender process, the attachment owns the Mach port of a newly created
  // message. The attachment broker will eventually take ownership of
  // |mach_port_|.
  // In the destination process, the attachment owns |mach_port_| until
  // ParamTraits<MachPortMac>::Read() is called, which takes ownership.
  bool owns_mach_port_;
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_MACH_PORT_ATTACHMENT_MAC_H_
