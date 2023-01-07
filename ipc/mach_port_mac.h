// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MACH_PORT_MAC_H_
#define IPC_MACH_PORT_MAC_H_

#include <mach/mach.h>

#include "base/pickle.h"
#include "ipc/ipc_message_support_export.h"
#include "ipc/ipc_param_traits.h"

namespace IPC {

// MachPortMac is a wrapper around an OSX Mach port that can be transported
// across Chrome IPC channels that support attachment brokering. The send right
// to the Mach port will be duplicated into the destination process by the
// attachment broker. If needed, attachment brokering can be trivially extended
// to support duplication of other types of rights.
class IPC_MESSAGE_SUPPORT_EXPORT MachPortMac {
 public:
  MachPortMac() : mach_port_(MACH_PORT_NULL) {}

  explicit MachPortMac(mach_port_t mach_port) : mach_port_(mach_port) {}

  MachPortMac(const MachPortMac&) = delete;
  MachPortMac& operator=(const MachPortMac&) = delete;

  mach_port_t get_mach_port() const { return mach_port_; }

  // This method should only be used by ipc/ translation code.
  void set_mach_port(mach_port_t mach_port) { mach_port_ = mach_port; }

 private:
  // The ownership semantics of |mach_port_| cannot be easily expressed with a
  // C++ scoped object. This is partly due to the mechanism by which Mach ports
  // are brokered, and partly due to the architecture of Chrome IPC.
  //
  // The broker for Mach ports may live in a different process than the sender
  // of the original Chrome IPC. In this case, it is signalled asynchronously,
  // and ownership of the Mach port passes from the sender process into the
  // broker process.
  //
  // Chrome IPC is written with the assumption that translation is a stateless
  // process. There is no good mechanism to convey the semantics of ownership
  // transfer from the Chrome IPC stack into the client code that receives the
  // translated message. As a result, Chrome IPC code assumes that every message
  // has a handler, and that the handler will take ownership of the Mach port.
  // Note that the same holds true for POSIX fds and Windows HANDLEs.
  //
  // When used by client code in the sender process, this class is just a
  // wrapper. The client code calls Send(new Message(MachPortMac(mach_port)))
  // and continues on its merry way. Behind the scenes, a MachPortAttachmentMac
  // takes ownership of the Mach port. When the attachment broker sends the name
  // of the Mach port to the broker process, it also releases
  // MachPortAttachmentMac's reference to the Mach port, as ownership has
  // effectively been transferred to the broker process.
  //
  // The broker process receives the name, duplicates the Mach port into the
  // destination process, and then decrements the ref count in the original
  // process.
  //
  // In the destination process, the attachment broker is responsible for
  // coupling the Mach port (inserted by the broker process) with Chrome IPC in
  // the form of a MachPortAttachmentMac. Due to the Chrome IPC translation
  // semantics discussed above, this MachPortAttachmentMac does not take
  // ownership of the Mach port, and assumes that the client code which receives
  // the callback will take ownership of the Mach port.
  mach_port_t mach_port_;
};

template <>
struct IPC_MESSAGE_SUPPORT_EXPORT ParamTraits<MachPortMac> {
  typedef MachPortMac param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // IPC_MACH_PORT_MAC_H_
