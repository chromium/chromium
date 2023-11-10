// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
#define GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/gpu_export.h"

namespace gpu {

class GPU_EXPORT ClientSharedImage
    : public base::RefCountedThreadSafe<ClientSharedImage> {
 public:
  explicit ClientSharedImage(const Mailbox& mailbox) : mailbox_(mailbox) {}

  const Mailbox& mailbox() { return mailbox_; }

 private:
  friend class base::RefCountedThreadSafe<ClientSharedImage>;
  ~ClientSharedImage() = default;

  const Mailbox mailbox_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_CLIENT_SHARED_IMAGE_H_
