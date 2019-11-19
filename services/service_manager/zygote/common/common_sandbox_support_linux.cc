// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/zygote/common/common_sandbox_support_linux.h"

#include "base/pickle.h"
#include "base/posix/global_descriptors.h"
#include "base/posix/unix_domain_socket.h"
#include "services/service_manager/embedder/descriptors.h"
#include "services/service_manager/sandbox/linux/sandbox_linux.h"

namespace service_manager {

#if !defined(OS_NACL_NONSFI)
int SharedMemoryIPCSupport::MakeSharedMemorySegment(size_t length,
                                                    bool executable) {
  base::Pickle request;
  request.WriteInt(
      service_manager::SandboxLinux::METHOD_MAKE_SHARED_MEMORY_SEGMENT);
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
#endif

int GetSandboxFD() {
  return service_manager::kSandboxIPCChannel +
         base::GlobalDescriptors::kBaseDescriptor;
}

}  // namespace service_manager
