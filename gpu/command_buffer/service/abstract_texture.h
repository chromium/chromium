// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_
#define GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "gpu/command_buffer/service/texture_base.h"
#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef unsigned GLenum;
typedef int GLsizei;
typedef int GLint;
typedef unsigned int GLuint;

namespace gpu {

namespace gles2 {

// An AbstractTexture enables access to GL textures from the GPU process, for
// things that set up textures using some client's decoder.  Creating an
// AbstractTexture is similar to "glGenTexture", and deleting it is similar to
// calling "glDeleteTextures".
//
// There are some subtle differences.  Deleting an AbstractTexture doesn't
// guarantee that the underlying platform texture has been deleted if it's
// referenced elsewhere.  For example, if it has been sent via mailbox to some
// other context, then it might still be around after the AbstractTexture has
// been destroyed.
//
// Also, an AbstractTexture is tied to the decoder that created it, in the sense
// that destroying the decoder drops the reference to the texture just as if the
// AbstractTexture were destroyed.  While it's okay for the AbstractTexture to
// exist beyond decoder destruction, it won't actually refer to a texture after
// that.  This makes it easier for the holder to ignore stub destruction; the
// texture will be cleaned up properly, as needed.
class GPU_GLES2_EXPORT AbstractTexture {
 public:
  using CleanupCallback = base::OnceCallback<void(AbstractTexture*)>;

  // The texture is guaranteed to be around while |this| exists, as long as
  // the decoder isn't destroyed / context isn't lost.
  virtual ~AbstractTexture() = default;

  // Return our TextureBase, useful mostly for creating a mailbox.  This may
  // return null if the texture has been destroyed.
  virtual TextureBase* GetTextureBase() const = 0;

  // Set a texture parameter.  The GL context must be current.
  virtual void SetParameteri(GLenum pname, GLint param) = 0;

  // Marks the texture as cleared, to help prevent sending an uninitialized
  // texture to the (untrusted) renderer.  One should call this only when one
  // has actually initialized the texture.
  virtual void SetCleared() = 0;

  // Set a callback that will be called when the AbstractTexture is going to
  // drop its reference to the underlying TextureBase.  We can't guarantee that
  // the TextureBase will be destroyed, but it is the last time that we can
  // guarantee that it won't be.  Typically, this callback will happen when the
  // AbstractTexture is destroyed, or when our stub is destroyed.  Do not change
  // the current context during this callback.  Also, do not assume that one
  // has a current context.
  virtual void SetCleanupCallback(CleanupCallback cleanup_callback) = 0;

  // Used to notify the AbstractTexture if the context is lost.
  virtual void NotifyOnContextLost() = 0;

  unsigned int service_id() const { return GetTextureBase()->service_id(); }
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ABSTRACT_TEXTURE_H_
