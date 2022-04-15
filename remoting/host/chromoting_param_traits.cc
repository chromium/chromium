// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_param_traits.h"

#include <stdint.h>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "ipc/ipc_message_protobuf_utils.h"
#include "ipc/ipc_message_utils.h"
#include "remoting/protocol/file_transfer_helpers.h"

namespace IPC {

// remoting::protocol::KeyboardLayout

// static
void ParamTraits<remoting::protocol::KeyboardLayout>::Write(
    base::Pickle* m,
    const remoting::protocol::KeyboardLayout& p) {
  std::string serialized_keyboard_layout;
  bool result = p.SerializeToString(&serialized_keyboard_layout);
  DCHECK(result);
  m->WriteString(serialized_keyboard_layout);
}

// static
bool ParamTraits<remoting::protocol::KeyboardLayout>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    remoting::protocol::KeyboardLayout* p) {
  std::string serialized_keyboard_layout;
  if (!iter->ReadString(&serialized_keyboard_layout))
    return false;

  return p->ParseFromString(serialized_keyboard_layout);
}

// static
void ParamTraits<remoting::protocol::KeyboardLayout>::Log(
    const remoting::protocol::KeyboardLayout& p,
    std::string* l) {
  l->append("[protocol::KeyboardLayout]");
}

// remoting::protocol::FileTransfer_Error

// static
void IPC::ParamTraits<remoting::protocol::FileTransfer_Error>::Write(
    base::Pickle* m,
    const param_type& p) {
  std::string serialized_file_transfer_error;
  bool result = p.SerializeToString(&serialized_file_transfer_error);
  DCHECK(result);
  m->WriteString(serialized_file_transfer_error);
}

// static
bool ParamTraits<remoting::protocol::FileTransfer_Error>::Read(
    const base::Pickle* m,
    base::PickleIterator* iter,
    param_type* p) {
  std::string serialized_file_transfer_error;
  if (!iter->ReadString(&serialized_file_transfer_error))
    return false;

  return p->ParseFromString(serialized_file_transfer_error);
}

// static
void ParamTraits<remoting::protocol::FileTransfer_Error>::Log(
    const param_type& p,
    std::string* l) {
  std::ostringstream formatted;
  formatted << p;
  l->append(
      base::StringPrintf("FileTransfer Error: %s", formatted.str().c_str()));
}

// remoting::Monostate

// static
void IPC::ParamTraits<remoting::Monostate>::Write(base::Pickle*,
                                                  const param_type&) {}

// static
bool ParamTraits<remoting::Monostate>::Read(const base::Pickle*,
                                            base::PickleIterator*,
                                            param_type*) {
  return true;
}

// static
void ParamTraits<remoting::Monostate>::Log(const param_type&, std::string* l) {
  l->append("()");
}

}  // namespace IPC
