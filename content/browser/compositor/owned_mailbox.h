// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_
#define CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "ui/compositor/compositor.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace content {

// This class holds a texture id and gpu::Mailbox, and deletes the texture
// id when the object itself is destroyed.
class CONTENT_EXPORT OwnedMailbox : public base::RefCounted<OwnedMailbox> {
 public:
  explicit OwnedMailbox(gpu::gles2::GLES2Interface* gl);

  const gpu::MailboxHolder& holder() const { return mailbox_holder_; }
  const gpu::Mailbox& mailbox() const { return mailbox_holder_.mailbox; }
  const gpu::SyncToken& sync_token() const {
    return mailbox_holder_.sync_token;
  }
  uint32_t texture_id() const { return texture_id_; }

  // Returns a SingleReleaseCallback for TextureLayer.
  std::unique_ptr<viz::SingleReleaseCallback> GetSingleReleaseCallback();

 private:
  friend class base::RefCounted<OwnedMailbox>;

  ~OwnedMailbox();

  void UpdateSyncToken(const gpu::SyncToken& sync_token, bool is_lost);

  gpu::gles2::GLES2Interface* const gl_;
  uint32_t texture_id_;
  gpu::MailboxHolder mailbox_holder_;
  base::WeakPtrFactory<OwnedMailbox> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_OWNED_MAILBOX_H_
