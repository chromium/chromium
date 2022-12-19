// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_
#define GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_

#include "gpu/gpu_export.h"
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_param_traits.h"

namespace gpu {
struct Mailbox;
struct MailboxHolder;
struct SyncToken;
struct VulkanYCbCrInfo;
}

namespace IPC {

template <>
struct GPU_EXPORT ParamTraits<gpu::SyncToken> {
  using param_type = gpu::SyncToken;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template<>
struct GPU_EXPORT ParamTraits<gpu::Mailbox> {
  using param_type = gpu::Mailbox;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GPU_EXPORT ParamTraits<gpu::MailboxHolder> {
  using param_type = gpu::MailboxHolder;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GPU_EXPORT ParamTraits<gpu::VulkanYCbCrInfo> {
  using param_type = gpu::VulkanYCbCrInfo;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

template <>
struct GPU_EXPORT ParamTraits<gpu::GpuMemoryBufferFormatSet> {
  typedef gpu::GpuMemoryBufferFormatSet param_type;
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* r);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // GPU_IPC_COMMON_GPU_COMMAND_BUFFER_TRAITS_H_
