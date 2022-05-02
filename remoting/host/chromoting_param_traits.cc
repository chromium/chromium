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

}  // namespace IPC
