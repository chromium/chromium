// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/gpu_command_buffer_traits.h"

#include <stddef.h>
#include <stdint.h>

#include "gpu/command_buffer/common/command_buffer_id.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

// Generate param traits log methods.
#include "ipc/param_traits_log_macros.h"
namespace IPC {
#include "gpu/ipc/common/gpu_command_buffer_traits_multi.h"
}  // namespace IPC

namespace IPC {

void ParamTraits<gpu::SyncToken>::Write(base::Pickle* m, const param_type& p) {
  DCHECK(!p.HasData() || p.verified_flush());

  WriteParam(m, p.verified_flush());
  WriteParam(m, p.namespace_id());
  WriteParam(m, p.command_buffer_id());
  WriteParam(m, p.release_count());
}

bool ParamTraits<gpu::SyncToken>::Read(const base::Pickle* m,
                                       base::PickleIterator* iter,
                                       param_type* p) {
  bool verified_flush = false;
  gpu::CommandBufferNamespace namespace_id =
      gpu::CommandBufferNamespace::INVALID;
  gpu::CommandBufferId command_buffer_id;
  uint64_t release_count = 0;

  if (!ReadParam(m, iter, &verified_flush) ||
      !ReadParam(m, iter, &namespace_id) ||
      !ReadParam(m, iter, &command_buffer_id) ||
      !ReadParam(m, iter, &release_count)) {
    return false;
  }

  p->Set(namespace_id, command_buffer_id, release_count);
  if (p->HasData()) {
    if (!verified_flush)
      return false;
    p->SetVerifyFlush();
  }

  return true;
}

void ParamTraits<gpu::SyncToken>::Log(const param_type& p, std::string* l) {
  *l += base::StringPrintf(
      "[%" PRId8 ":%" PRIX64 "] %" PRIu64, p.namespace_id(),
      p.command_buffer_id().GetUnsafeValue(), p.release_count());
}

void ParamTraits<gpu::TextureInUseResponse>::Write(base::Pickle* m,
                                                   const param_type& p) {
  WriteParam(m, p.texture);
  WriteParam(m, p.in_use);
}

bool ParamTraits<gpu::TextureInUseResponse>::Read(const base::Pickle* m,
                                                  base::PickleIterator* iter,
                                                  param_type* p) {
  uint32_t texture = 0;
  bool in_use = false;

  if (!ReadParam(m, iter, &texture) || !ReadParam(m, iter, &in_use)) {
    return false;
  }

  p->texture = texture;
  p->in_use = in_use;
  return true;
}

void ParamTraits<gpu::TextureInUseResponse>::Log(const param_type& p,
                                                 std::string* l) {
  *l += base::StringPrintf("[%u: %d]", p.texture, static_cast<int>(p.in_use));
}

void ParamTraits<gpu::Mailbox>::Write(base::Pickle* m, const param_type& p) {
  m->WriteBytes(p.name, sizeof(p.name));
}

bool ParamTraits<gpu::Mailbox>::Read(const base::Pickle* m,
                                     base::PickleIterator* iter,
                                     param_type* p) {
  const char* bytes = nullptr;
  if (!iter->ReadBytes(&bytes, sizeof(p->name)))
    return false;
  DCHECK(bytes);
  memcpy(p->name, bytes, sizeof(p->name));
  return true;
}

void ParamTraits<gpu::Mailbox>::Log(const param_type& p, std::string* l) {
  for (size_t i = 0; i < sizeof(p.name); ++i)
    *l += base::StringPrintf("%02x", p.name[i]);
}

void ParamTraits<gpu::MailboxHolder>::Write(base::Pickle* m,
                                            const param_type& p) {
  WriteParam(m, p.mailbox);
  WriteParam(m, p.sync_token);
  WriteParam(m, p.texture_target);
}

bool ParamTraits<gpu::MailboxHolder>::Read(const base::Pickle* m,
                                           base::PickleIterator* iter,
                                           param_type* p) {
  if (!ReadParam(m, iter, &p->mailbox) || !ReadParam(m, iter, &p->sync_token) ||
      !ReadParam(m, iter, &p->texture_target))
    return false;
  return true;
}

void ParamTraits<gpu::MailboxHolder>::Log(const param_type& p, std::string* l) {
  LogParam(p.mailbox, l);
  LogParam(p.sync_token, l);
  *l += base::StringPrintf(":%04x@", p.texture_target);
}

void ParamTraits<gpu::VulkanYCbCrInfo>::Write(base::Pickle* m,
                                              const param_type& p) {
  WriteParam(m, p.image_format);
  WriteParam(m, p.external_format);
  WriteParam(m, p.suggested_ycbcr_model);
  WriteParam(m, p.suggested_ycbcr_range);
  WriteParam(m, p.suggested_xchroma_offset);
  WriteParam(m, p.suggested_ychroma_offset);
  WriteParam(m, p.format_features);
}

bool ParamTraits<gpu::VulkanYCbCrInfo>::Read(const base::Pickle* m,
                                             base::PickleIterator* iter,
                                             param_type* p) {
  if (!ReadParam(m, iter, &p->image_format) ||
      !ReadParam(m, iter, &p->external_format) ||
      !ReadParam(m, iter, &p->suggested_ycbcr_model) ||
      !ReadParam(m, iter, &p->suggested_ycbcr_range) ||
      !ReadParam(m, iter, &p->suggested_xchroma_offset) ||
      !ReadParam(m, iter, &p->suggested_ychroma_offset) ||
      !ReadParam(m, iter, &p->format_features)) {
    return false;
  }
  return true;
}

// Note that we are casting uint64_t explicitly to long long otherwise it gets
// implicit cast to long for 64 bit OS and long long for 32 bit OS.
void ParamTraits<gpu::VulkanYCbCrInfo>::Log(const param_type& p,
                                            std::string* l) {
  *l += base::StringPrintf(
      "[%u] , [%llu], [%u], [%u], [%u], [%u], [%u]", p.image_format,
      static_cast<long long>(p.external_format), p.suggested_ycbcr_model,
      p.suggested_ycbcr_range, p.suggested_xchroma_offset,
      p.suggested_ychroma_offset, p.format_features);
}

}  // namespace IPC
