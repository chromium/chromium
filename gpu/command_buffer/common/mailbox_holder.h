// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_COMMON_MAILBOX_HOLDER_H_
#define GPU_COMMAND_BUFFER_COMMON_MAILBOX_HOLDER_H_

#include <stdint.h>
#include <string.h>

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/gpu_export.h"

namespace gpu {

// A MailboxHolder is a mechanism by which texture image data produced by one
// context can be consumed by another. The |sync_point| is used to allow one
// context to wait until another has finished using the texture before it begins
// using the texture. When the mailbox is backed by a GPU texture, the
// |texture_target| is that texture's type.
// See here for OpenGL texture types:
// https://www.opengl.org/wiki/Texture#Texture_Objects
struct GPU_EXPORT MailboxHolder {
  MailboxHolder();
  MailboxHolder(const gpu::Mailbox& mailbox,
                const gpu::SyncToken& sync_token,
                uint32_t texture_target);

  gpu::Mailbox mailbox;
  gpu::SyncToken sync_token;
  uint32_t texture_target;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_MAILBOX_HOLDER_H_
