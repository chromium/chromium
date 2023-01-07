// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/zygote/sandbox_support_linux.h"

#include "base/pickle.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "content/public/common/content_descriptors.h"
#include "sandbox/policy/linux/sandbox_linux.h"

namespace content {

int SharedMemoryIPCSupport::MakeSharedMemorySegment(size_t length,
                                                    bool executable) {
  base::Pickle request;
  request.WriteInt(
      sandbox::policy::SandboxLinux::METHOD_MAKE_SHARED_MEMORY_SEGMENT);
  request.WriteUInt32(length);
  request.WriteBool(executable);
  uint8_t reply_buf[10];
  int result_fd;
  ssize_t result = base::UnixDomainSocket::SendRecvMsg(
      GetSandboxFD(), reply_buf, sizeof(reply_buf), &result_fd, request);
  if (result == -1)
    return -1;
  return result_fd;
}

int GetSandboxFD() {
  return kSandboxIPCChannel + base::GlobalDescriptors::kBaseDescriptor;
}

}  // namespace content
