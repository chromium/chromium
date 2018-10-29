// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_

#include <stdint.h>

#include "gpu/gpu_export.h"

namespace gpu {

class MailboxManager;

class GPU_EXPORT TextureBase {
 public:
  explicit TextureBase(unsigned int service_id);
  virtual ~TextureBase();

  // The service side OpenGL id of the texture.
  unsigned int service_id() const { return service_id_; }

  // Returns the target this texure was first bound to or 0 if it has not
  // been bound. Once a texture is bound to a specific target it can never be
  // bound to a different target.
  unsigned int target() const { return target_; }

  void SetMailboxManager(MailboxManager* mailbox_manager);
  MailboxManager* mailbox_manager() const { return mailbox_manager_; }

  // An identifier for subclasses. Necessary for safe downcasting.
  enum class Type { kNone, kValidated, kPassthrough };
  virtual Type GetType() const;

 protected:
  // The id of the texture.
  unsigned int service_id_;

  // The target. 0 if unset, otherwise GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.
  //             Or GL_TEXTURE_2D_ARRAY or GL_TEXTURE_3D (for GLES3).
  unsigned int target_;

  void SetTarget(unsigned int target);

  void DeleteFromMailboxManager();

 private:
  MailboxManager* mailbox_manager_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_BASE_H_
