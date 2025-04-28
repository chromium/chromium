/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_OBJECT_H_

#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {

class WebGLContextGroup;
class WebGLRenderingContextBase;

template <typename T>
GLuint ObjectOrZero(const T* object) {
  return object ? object->Object() : 0;
}

template <typename T>
GLuint ObjectNonZero(const T* object) {
  GLuint result = object->Object();
  DCHECK(result);
  return result;
}

class WebGLObject : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

  // WebGLObjects are pre-finalized, and the WebGLRenderingContextBase
  // is specifically not. This is done in order to allow WebGLObjects to
  // refer back to their owning context in their destructor to delete their
  // resources if they are GC'd before the context is.
  USING_PRE_FINALIZER(WebGLObject, Dispose);

 public:
  WebGLObject(const WebGLObject&) = delete;
  WebGLObject& operator=(const WebGLObject&) = delete;

  // We can't call virtual functions like deleteObjectImpl in this class's
  // destructor; doing so results in a pure virtual function call. Further,
  // making this destructor non-virtual is complicated with respect to
  // Oilpan tracing. Therefore this destructor is declared virtual, but is
  // empty, and the code that would have gone into its body is called by
  // subclasses via Dispose().
  ~WebGLObject() override;

  // deleteObject may not always delete the OpenGL resource.  For programs and
  // shaders, deletion is delayed until they are no longer attached.
  // FIXME: revisit this when resource sharing between contexts are implemented.
  void DeleteObject(gpu::gles2::GLES2Interface*);

  void OnAttached() { ++attachment_count_; }
  void OnDetached(gpu::gles2::GLES2Interface*);

  // This indicates whether the client side has already issued a delete call,
  // not whether the OpenGL resource is deleted. Object()==0, or !HasObject(),
  // indicates that the OpenGL resource has been deleted.
  bool MarkedForDeletion() { return marked_for_deletion_; }

  // True if this object belongs to the group or context.
  virtual bool Validate(const WebGLContextGroup*,
                        const WebGLRenderingContextBase*) const = 0;
  virtual bool HasObject() const = 0;

 protected:
  explicit WebGLObject(WebGLRenderingContextBase*);

  // deleteObjectImpl should be only called once to delete the OpenGL resource.
  // After calling deleteObjectImpl, hasObject() should return false.
  virtual void DeleteObjectImpl(gpu::gles2::GLES2Interface*) = 0;

  virtual bool HasGroupOrContext() const = 0;

  // Return the current number of context losses associated with this
  // object's context group (if it's a shared object), or its
  // context's context group (if it's a per-context object).
  virtual uint32_t CurrentNumberOfContextLosses() const = 0;

  uint32_t CachedNumberOfContextLosses() const;

  void Detach();
  void DetachAndDeleteObject();

  virtual gpu::gles2::GLES2Interface* GetAGLInterface() const = 0;

  // Runs the pre-finalization sequence -- what would be in the destructor
  // of the base class, if it could be. Must be called no more than once.
  void Dispose();

  // Indicates to subclasses that the destructor is being run.
  bool DestructionInProgress() const;

 private:
  // This was the number of context losses of the object's associated
  // WebGLContextGroup at the time this object was created. Contexts
  // no longer refer to all the objects that they ever created, so
  // it's necessary to check this count when validating each object.
  uint32_t cached_number_of_context_losses_;

  unsigned attachment_count_;

  // Indicates whether the WebGL context's deletion function for this object
  // (deleteBuffer, deleteTexture, etc.) has been called. It does *not* indicate
  // whether the underlying OpenGL resource has been destroyed; !HasObject()
  // indicates that.
  bool marked_for_deletion_;

  // Indicates whether the destructor has been entered and we therefore
  // need to be careful in subclasses to not touch other on-heap objects.
  bool destruction_in_progress_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGL_WEBGL_OBJECT_H_
