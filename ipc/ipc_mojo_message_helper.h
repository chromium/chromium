// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_MOJO_MESSAGE_HELPER_H_
#define IPC_IPC_MOJO_MESSAGE_HELPER_H_

#include "ipc/ipc_message.h"
#include "ipc/ipc_message_support_export.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace IPC {

// Reads and writes |mojo::MessagePipe| from/to |Message|.
class IPC_MESSAGE_SUPPORT_EXPORT MojoMessageHelper {
 public:
  static bool WriteMessagePipeTo(base::Pickle* message,
                                 mojo::ScopedMessagePipeHandle handle);
  static bool ReadMessagePipeFrom(const base::Pickle* message,
                                  base::PickleIterator* iter,
                                  mojo::ScopedMessagePipeHandle* handle);

 private:
  MojoMessageHelper();
};

}  // namespace IPC

#endif  // IPC_IPC_MOJO_MESSAGE_HELPER_H_
