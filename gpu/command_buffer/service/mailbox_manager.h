// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"

namespace gpu {

class TextureBase;

// Manages resources scoped beyond the context or context group level.
class GPU_EXPORT MailboxManager {
 public:
  virtual ~MailboxManager() = default;

  // Look up the texture definition from the named mailbox.
  virtual TextureBase* ConsumeTexture(const Mailbox& mailbox) = 0;

  // Put the texture into the named mailbox.
  virtual void ProduceTexture(const Mailbox& mailbox, TextureBase* texture) = 0;

  // Destroy any mailbox that reference the given texture.
  virtual void TextureDeleted(TextureBase* texture) = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

