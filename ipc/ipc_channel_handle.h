// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_HANDLE_H_
#define IPC_IPC_CHANNEL_HANDLE_H_

#include "build/build_config.h"
#include "mojo/public/cpp/system/message_pipe.h"

#if BUILDFLAG(IS_NACL)
#include "base/file_descriptor_posix.h"
#endif  // defined (OS_NACL)

namespace IPC {

// Note that serialization for this object is defined in the ParamTraits
// template specialization in ipc_message_utils.h.
#if BUILDFLAG(IS_NACL)
struct ChannelHandle {
  ChannelHandle() {}
  explicit ChannelHandle(const base::FileDescriptor& s) : socket(s) {}

  base::FileDescriptor socket;
};
#else
struct ChannelHandle {
  ChannelHandle() {}
  ChannelHandle(mojo::MessagePipeHandle h) : mojo_handle(h) {}

  bool is_mojo_channel_handle() const { return mojo_handle.is_valid(); }

  mojo::MessagePipeHandle mojo_handle;
};
#endif  // BUILDFLAG(IS_NACL)

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_HANDLE_H_
