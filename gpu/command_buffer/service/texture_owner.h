// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_TEXTURE_OWNER_H_
#define GPU_COMMAND_BUFFER_SERVICE_TEXTURE_OWNER_H_

#include <android/hardware_buffer.h>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/gpu_gles2_export.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gpu {
class AbstractTextureAndroid;
class TextureBase;

// Used for diagnosting metrics. Do not use for anything else.
// TODO(crbug.com/329821776): Remove once we get enough data.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum TextureOwnerCodecType {
  kMediaCodec = 0,
  kStreamTexture = 1,
  kMaxValue = kStreamTexture
};

// A Texture wrapper interface that creates and maintains ownership of the
// attached GL or Vulkan texture. The texture is destroyed with the object.
// It should only be accessed on the thread it was created on, with the
// exception of CreateJavaSurface() and SetFrameAvailableCallback(), which can
// be called on any thread. It's safe to keep and drop refptrs to it on any
// thread; it will be automatically destructed on the thread it was constructed
// on.
// TextureOwner also is a shared context lost observer to get notified if the
// TextureOwner's shared context is lost.
class GPU_GLES2_EXPORT TextureOwner
    : public base::RefCountedDeleteOnSequence<TextureOwner>,
      public SharedContextState::ContextLostObserver,
      public base::trace_event::MemoryDumpProvider {
 public:
  // Creates a GL texture using the current platform GL context and returns a
  // new TextureOwner attached to it. Returns null on failure.
  // |texture| should be either from CreateAbstractTexture() or a mock.  The
  // corresponding GL context must be current.
  // Mode indicates which framework API to use and whether the video textures
  // created using this owner should be hardware protected. It also indicates
  // whether SurfaceControl is being used or not.
  enum class Mode {
    kAImageReaderInsecure,
    kAImageReaderInsecureSurfaceControl,
    kAImageReaderSecureSurfaceControl,
    kSurfaceTextureInsecure
  };

  static scoped_refptr<TextureOwner> Create(
      Mode mode,
      scoped_refptr<SharedContextState> context_state,
      scoped_refptr<RefCountedLock> drdc_lock,
      TextureOwnerCodecType type_for_metrics);

  TextureOwner(const TextureOwner&) = delete;
  TextureOwner& operator=(const TextureOwner&) = delete;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return task_runner_;
  }

  // Returns the GL texture id that the TextureOwner is attached to.
  GLuint GetTextureId() const;
  TextureBase* GetTextureBase() const;
  virtual gl::GLContext* GetContext() const = 0;
  virtual gl::GLSurface* GetSurface() const = 0;

  // Create a java surface for the TextureOwner.
  virtual gl::ScopedJavaSurface CreateJavaSurface() const = 0;

  // Update the texture image using the latest available image data.
  virtual void UpdateTexImage() = 0;

  // Transformation matrix if any associated with the texture image.
  virtual void ReleaseBackBuffers() = 0;

  // Retrieves the AHardwareBuffer from the latest available image data.
  // Note that the object must be used and destroyed on the same thread the
  // TextureOwner is bound to.
  virtual std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() = 0;

  // Retrieves backing size and visible rect associated with the most recent
  // image. |rotated_visible_size| is the size of the visible region
  // post-transform in pixels and is used for SurfaceTexture case. Transform
  // here means transform that we get from SurfaceTexture. For MediaPlayer we
  // expect to have rotation and MediaPlayer reports rotated size. For
  // MediaCodec we don't expect rotation in ST so visible_size (i.e crop rect
  // from codec) can be used.
  // Returns whether call was successful or not.
  virtual bool GetCodedSizeAndVisibleRect(gfx::Size rotated_visible_size,
                                          gfx::Size* coded_size,
                                          gfx::Rect* visible_rect) = 0;

  // Set the callback function to run when a new frame is available.
  // |frame_available_cb| is thread safe and can be called on any thread. This
  // method should be called only once, i.e., once a callback is provided, it
  // should not be changed.
  virtual void SetFrameAvailableCallback(
      const base::RepeatingClosure& frame_available_cb) = 0;

  // Runs callback when the free buffer is available to render to front buffer.
  // Can be run before returning from the function. Callback is run on a caller
  // thread.
  virtual void RunWhenBufferIsAvailable(base::OnceClosure callback) = 0;

  bool binds_texture_on_update() const { return binds_texture_on_update_; }

  // SharedContextState::ContextLostObserver implementation.
  void OnContextLost() override;

 protected:
  friend class base::RefCountedDeleteOnSequence<TextureOwner>;
  friend class base::DeleteHelper<TextureOwner>;

  // |texture| is the texture that we'll own.
  TextureOwner(bool binds_texture_on_update,
               std::unique_ptr<AbstractTextureAndroid> texture,
               scoped_refptr<SharedContextState> context_state);
  ~TextureOwner() override;

  // Called when |texture_| signals that the platform texture will be destroyed.
  virtual void ReleaseResources() = 0;

  AbstractTextureAndroid* texture() const { return texture_.get(); }

  int tracing_id() const { return tracing_id_; }

 private:
  friend class MockTextureOwner;

  // To be used by MockTextureOwner.
  TextureOwner(bool binds_texture_on_update,
               std::unique_ptr<AbstractTextureAndroid> texture);

  // Set to true if the updating the image for this owner will automatically
  // bind it to the texture target.
  const bool binds_texture_on_update_;

  scoped_refptr<SharedContextState> context_state_;
  std::unique_ptr<AbstractTextureAndroid> texture_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const int tracing_id_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_TEXTURE_OWNER_H_
