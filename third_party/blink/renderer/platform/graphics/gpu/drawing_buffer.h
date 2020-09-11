/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/macros.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgl_image_conversion.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types_3d.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gpu_preference.h"

namespace cc {
class Layer;
}

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}

namespace blink {
class CanvasColorParams;
class CanvasResource;
class CanvasResourceProvider;
class Extensions3DUtil;
class StaticBitmapImage;
class WebGraphicsContext3DProvider;
class WebGraphicsContext3DProviderWrapper;

// Manages a rendering target (framebuffer + attachment) for a canvas.  Can
// publish its rendering results to a cc::Layer for compositing.
class PLATFORM_EXPORT DrawingBuffer : public cc::TextureLayerClient,
                                      public RefCounted<DrawingBuffer> {

 public:
  class Client {
   public:
    // Returns true if the DrawingBuffer is currently bound for draw.
    virtual bool DrawingBufferClientIsBoundForDraw() = 0;
    virtual void DrawingBufferClientRestoreScissorTest() = 0;
    // Restores the mask and clear value for color, depth, and stencil buffers.
    virtual void DrawingBufferClientRestoreMaskAndClearValues() = 0;
    // Assume client knows the GL/WebGL version and restore necessary params
    // accordingly.
    virtual void DrawingBufferClientRestorePixelPackParameters() = 0;
    // Restores the GL_TEXTURE_2D binding for the active texture unit only.
    virtual void DrawingBufferClientRestoreTexture2DBinding() = 0;
    // Restores the GL_TEXTURE_CUBE_MAP binding for the active texture unit.
    virtual void DrawingBufferClientRestoreTextureCubeMapBinding() = 0;
    virtual void DrawingBufferClientRestoreRenderbufferBinding() = 0;
    virtual void DrawingBufferClientRestoreFramebufferBinding() = 0;
    virtual void DrawingBufferClientRestorePixelUnpackBufferBinding() = 0;
    virtual void DrawingBufferClientRestorePixelPackBufferBinding() = 0;
    virtual bool
    DrawingBufferClientUserAllocatedMultisampledRenderbuffers() = 0;
    virtual void DrawingBufferClientForceLostContextWithAutoRecovery() = 0;
  };

  enum PreserveDrawingBuffer {
    kPreserve,
    kDiscard,
  };
  enum WebGLVersion {
    kWebGL1,
    kWebGL2,
    kWebGL2Compute,
  };

  enum ChromiumImageUsage {
    kAllowChromiumImage,
    kDisallowChromiumImage,
  };

  static scoped_refptr<DrawingBuffer> Create(
      std::unique_ptr<WebGraphicsContext3DProvider>,
      bool using_gpu_compositing,
      bool using_swap_chain,
      Client*,
      const IntSize&,
      bool premultiplied_alpha,
      bool want_alpha_channel,
      bool want_depth_buffer,
      bool want_stencil_buffer,
      bool want_antialiasing,
      PreserveDrawingBuffer,
      WebGLVersion,
      ChromiumImageUsage,
      SkFilterQuality,
      const CanvasColorParams&,
      gl::GpuPreference);

  ~DrawingBuffer() override;

  // Destruction will be completed after all mailboxes are released.
  void BeginDestruction();

  // Issues a glClear() on all framebuffers associated with this DrawingBuffer.
  void ClearFramebuffers(GLbitfield clear_mask);

  // Indicates whether the DrawingBuffer internally allocated a packed
  // depth-stencil renderbuffer in the situation where the end user only asked
  // for a depth buffer. In this case, we need to upgrade clears of the depth
  // buffer to clears of the depth and stencil buffers in order to avoid
  // performance problems on some GPUs.
  bool HasImplicitStencilBuffer() const { return has_implicit_stencil_buffer_; }
  bool HasDepthBuffer() const { return !!depth_stencil_buffer_; }
  bool HasStencilBuffer() const { return !!depth_stencil_buffer_; }

  // Given the desired buffer size, provides the largest dimensions that will
  // fit in the pixel budget.
  static IntSize AdjustSize(const IntSize& desired_size,
                            const IntSize& cur_size,
                            int max_texture_size);

  // Resizes (or allocates if necessary) all buffers attached to the default
  // framebuffer. Returns whether the operation was successful.
  bool Resize(const IntSize&);

  // Bind the default framebuffer to |target|. |target| must be
  // GL_FRAMEBUFFER, GL_READ_FRAMEBUFFER, or GL_DRAW_FRAMEBUFFER.
  void Bind(GLenum target);
  IntSize Size() const { return size_; }

  // Resolves the multisample color buffer to the normal color buffer and leaves
  // the resolved color buffer bound to GL_READ_FRAMEBUFFER and
  // GL_DRAW_FRAMEBUFFER.
  void ResolveAndBindForReadAndDraw();

  bool Multisample() const;

  bool DiscardFramebufferSupported() const {
    return discard_framebuffer_supported_;
  }

  // Returns false if the contents had previously been marked as changed and
  // have not yet been resolved.
  bool MarkContentsChanged();

  // Maintenance of auto-clearing of color/depth/stencil buffers. The
  // Reset method is present to keep calling code simpler, so it
  // doesn't have to know which buffers were allocated.
  void ResetBuffersToAutoClear();
  void SetBuffersToAutoClear(GLbitfield bitmask);
  GLbitfield GetBuffersToAutoClear() const;

  void SetIsInHiddenPage(bool);
  void SetFilterQuality(SkFilterQuality);
  SkFilterQuality FilterQuality() const { return filter_quality_; }

  // Whether the target for draw operations has format GL_RGBA, but is
  // emulating format GL_RGB. When the target's storage is first
  // allocated, its alpha channel must be cleared to 1. All future drawing
  // operations must use a color mask with alpha=GL_FALSE.
  bool RequiresAlphaChannelToBePreserved();

  // Similar to requiresAlphaChannelToBePreserved(), but always targets the
  // default framebuffer.
  bool DefaultBufferRequiresAlphaChannelToBePreserved();

  cc::Layer* CcLayer();

  gpu::gles2::GLES2Interface* ContextGL();
  WebGraphicsContext3DProvider* ContextProvider();
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWeakPtr();
  Client* client() { return client_; }
  WebGLVersion webgl_version() const { return webgl_version_; }
  bool destroyed() const { return destruction_in_progress_; }

  // cc::TextureLayerClient implementation.
  bool PrepareTransferableResource(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback)
      override;

  // Returns a StaticBitmapImage backed by a texture containing the current
  // contents of the front buffer. This is done without any pixel copies. The
  // texture in the ImageBitmap is from the active ContextProvider on the
  // DrawingBuffer.
  scoped_refptr<StaticBitmapImage> TransferToStaticBitmapImage();

  bool CopyToPlatformTexture(gpu::gles2::GLES2Interface*,
                             GLenum dst_target,
                             GLuint dst_texture,
                             GLint dst_level,
                             bool premultiply_alpha,
                             bool flip_y,
                             const IntPoint& dst_texture_offset,
                             const IntRect& src_sub_rectangle,
                             SourceDrawingBuffer);

  bool CopyToPlatformMailbox(gpu::raster::RasterInterface*,
                             gpu::Mailbox dst_mailbox,
                             GLenum dst_texture_target,
                             bool flip_y,
                             const IntPoint& dst_texture_offset,
                             const IntRect& src_sub_rectangle,
                             SourceDrawingBuffer src_buffer);

  sk_sp<SkData> PaintRenderingResultsToDataArray(SourceDrawingBuffer);

  int SampleCount() const { return sample_count_; }
  bool ExplicitResolveOfMultisampleData() const {
    return anti_aliasing_mode_ == kAntialiasingModeMSAAExplicitResolve;
  }

  // Rebind the read and draw framebuffers that WebGL is expecting.
  void RestoreFramebufferBindings();

  // Restore all state that may have been dirtied by any call.
  void RestoreAllState();

  bool UsingSwapChain() const { return using_swap_chain_; }

  // This class helps implement correct semantics for BlitFramebuffer
  // when the DrawingBuffer is using a CHROMIUM image for its backing
  // store and RGB emulation is in use (basically, macOS only).
  class PLATFORM_EXPORT ScopedRGBEmulationForBlitFramebuffer {
    STACK_ALLOCATED();

   public:
    ScopedRGBEmulationForBlitFramebuffer(DrawingBuffer*,
                                         bool is_user_draw_framebuffer_bound);
    ~ScopedRGBEmulationForBlitFramebuffer();

   private:
    scoped_refptr<DrawingBuffer> drawing_buffer_;
    bool doing_work_ = false;
  };

  scoped_refptr<CanvasResource> AsCanvasResource(
      base::WeakPtr<CanvasResourceProvider> resource_provider);

  static const size_t kDefaultColorBufferCacheLimit;

 protected:  // For unittests
  DrawingBuffer(std::unique_ptr<WebGraphicsContext3DProvider>,
                bool using_gpu_compositing,
                bool using_swap_chain,
                std::unique_ptr<Extensions3DUtil>,
                Client*,
                bool discard_framebuffer_supported,
                bool want_alpha_channel,
                bool premultiplied_alpha,
                PreserveDrawingBuffer,
                WebGLVersion,
                bool wants_depth,
                bool wants_stencil,
                ChromiumImageUsage,
                SkFilterQuality,
                const CanvasColorParams&,
                gl::GpuPreference gpu_preference);

  bool Initialize(const IntSize&, bool use_multisampling);

  struct RegisteredBitmap {
    scoped_refptr<cc::CrossThreadSharedBitmap> bitmap;
    cc::SharedBitmapIdRegistration registration;

    // Explicitly move-only.
    RegisteredBitmap(RegisteredBitmap&&) = default;
    RegisteredBitmap& operator=(RegisteredBitmap&&) = default;
  };
  // Shared memory bitmaps that were released by the compositor and can be used
  // again by this DrawingBuffer.
  Vector<RegisteredBitmap> recycled_bitmaps_;

 private:
  friend class ScopedRGBEmulationForBlitFramebuffer;
  friend class ScopedStateRestorer;
  friend class ColorBuffer;

  // This structure should wrap all public entrypoints that may modify GL state.
  // It will restore all state when it drops out of scope.
  class ScopedStateRestorer {
    USING_FAST_MALLOC(ScopedStateRestorer);

   public:
    ScopedStateRestorer(DrawingBuffer*);
    ~ScopedStateRestorer();

    // Mark parts of the state that are dirty and need to be restored.
    void SetClearStateDirty() { clear_state_dirty_ = true; }
    void SetPixelPackParametersDirty() { pixel_pack_parameters_dirty_ = true; }
    void SetTextureBindingDirty() { texture_binding_dirty_ = true; }
    void SetRenderbufferBindingDirty() { renderbuffer_binding_dirty_ = true; }
    void SetFramebufferBindingDirty() { framebuffer_binding_dirty_ = true; }
    void SetPixelUnpackBufferBindingDirty() {
      pixel_unpack_buffer_binding_dirty_ = true;
    }
    void SetPixelPackBufferBindingDirty() {
      pixel_pack_buffer_binding_dirty_ = true;
    }

   private:
    scoped_refptr<DrawingBuffer> drawing_buffer_;
    // The previous state restorer, in case restorers are nested.
    ScopedStateRestorer* previous_state_restorer_ = nullptr;
    bool clear_state_dirty_ = false;
    bool pixel_pack_parameters_dirty_ = false;
    bool texture_binding_dirty_ = false;
    bool renderbuffer_binding_dirty_ = false;
    bool framebuffer_binding_dirty_ = false;
    bool pixel_unpack_buffer_binding_dirty_ = false;
    bool pixel_pack_buffer_binding_dirty_ = false;
  };

  struct ColorBuffer : public base::RefCountedThreadSafe<ColorBuffer> {
    ColorBuffer(base::WeakPtr<DrawingBuffer> drawing_buffer,
                const IntSize&,
                viz::ResourceFormat,
                GLuint texture_id,
                std::unique_ptr<gfx::GpuMemoryBuffer>,
                gpu::Mailbox mailbox);
    ~ColorBuffer();

    // The thread on which the ColorBuffer is created and the DrawingBuffer is
    // bound to.
    const base::PlatformThreadRef owning_thread_ref;

    // The owning DrawingBuffer. Note that DrawingBuffer is explicitly destroyed
    // by the beginDestruction method, which will eventually drain all of its
    // ColorBuffers.
    base::WeakPtr<DrawingBuffer> drawing_buffer;
    const IntSize size;
    const viz::ResourceFormat format;
    const GLuint texture_id = 0;
    std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer;

    // If we're emulating an RGB back buffer using an RGBA Chromium
    // image (essentially macOS only), then when performing
    // BlitFramebuffer calls, we have to swap in an RGB texture in
    // place of the RGBA texture bound to the image. The reason is
    // that BlitFramebuffer requires the internal formats of the
    // source and destination to match (e.g. RGB8 on both sides).
    // There are bugs in the semantics of RGB8 textures in this
    // situation (the alpha channel is zeroed), requiring more fixups.
    GLuint rgb_workaround_texture_id = 0;

    // The mailbox used to send this buffer to the compositor.
    gpu::Mailbox mailbox;

    // The sync token for when this buffer was sent to the compositor.
    gpu::SyncToken produce_sync_token;

    // The sync token for when this buffer was received back from the
    // compositor.
    gpu::SyncToken receive_sync_token;

   private:
    DISALLOW_COPY_AND_ASSIGN(ColorBuffer);
  };

  template <typename CopyFunction>
  bool CopyToPlatformInternal(gpu::InterfaceBase* dst_interface,
                              SourceDrawingBuffer src_buffer,
                              const CopyFunction& copy_function);

  enum ClearOption { ClearOnlyMultisampledFBO, ClearAllFBOs };

  // Clears out newly-allocated framebuffers (really, renderbuffers / textures).
  void ClearNewlyAllocatedFramebuffers(ClearOption clear_option);

  // The same as clearFramebuffers(), but leaves GL state dirty.
  void ClearFramebuffersInternal(GLbitfield clear_mask,
                                 ClearOption clear_option);

  // The same as reset(), but leaves GL state dirty.
  bool ResizeFramebufferInternal(const IntSize&);

  // The same as resolveAndBindForReadAndDraw(), but leaves GL state dirty.
  void ResolveMultisampleFramebufferInternal();

  // Resolves m_multisampleFBO into m_fbo, if multisampling.
  void ResolveIfNeeded();

  bool PrepareTransferableResourceInternal(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback,
      bool force_gpu_result);

  // Helper functions to be called only by PrepareTransferableResourceInternal.
  bool FinishPrepareTransferableResourceGpu(
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);
  bool FinishPrepareTransferableResourceSoftware(
      cc::SharedBitmapIdRegistrar* bitmap_registrar,
      viz::TransferableResource* out_resource,
      std::unique_ptr<viz::SingleReleaseCallback>* out_release_callback);

  // Callbacks for mailboxes given to the compositor from
  // FinishPrepareTransferableResource{Gpu,Software}.
  static void NotifyMailboxReleasedGpu(scoped_refptr<ColorBuffer>,
                                       const gpu::SyncToken&,
                                       bool lost_resource);
  void MailboxReleasedGpu(scoped_refptr<ColorBuffer>,
                          bool lost_resource);
  void MailboxReleasedSoftware(RegisteredBitmap,
                               const gpu::SyncToken&,
                               bool lost_resource);

  // Attempts to allocator storage for, or resize all buffers. Returns whether
  // the operation was successful.
  bool ResizeDefaultFramebuffer(const IntSize&);

  void ClearCcLayer();

  RegisteredBitmap CreateOrRecycleBitmap(
      cc::SharedBitmapIdRegistrar* bitmap_registrar);

  // Updates the current size of the buffer, ensuring that
  // s_currentResourceUsePixels is updated.
  void SetSize(const IntSize&);

  // Helper function which does a readback from the currently-bound
  // framebuffer into a buffer of a certain size with 4-byte pixels.
  void ReadBackFramebuffer(base::span<uint8_t> pixels,
                           SkColorType,
                           WebGLImageConversion::AlphaOp);

  // If RGB emulation is required, then the CHROMIUM image's alpha channel
  // must be immediately cleared after it is bound to a texture. Nothing
  // should be allowed to change the alpha channel after this.
  void ClearChromiumImageAlpha(const ColorBuffer&);

  // Tries to create a CHROMIUM_image backed texture if
  // RuntimeEnabledFeatures::WebGLImageChromiumEnabled() is true. On failure,
  // or if the flag is false, creates a default texture. Always returns a valid
  // ColorBuffer.
  scoped_refptr<ColorBuffer> CreateColorBuffer(const IntSize&);

  // Creates or recycles a ColorBuffer of size |m_size|.
  scoped_refptr<ColorBuffer> CreateOrRecycleColorBuffer();

  // Attaches |m_backColorBuffer| to |m_fbo|, which is always the source for
  // read operations.
  void AttachColorBufferToReadFramebuffer();

  // Whether the WebGL client desires an explicit resolve. This is
  // implemented by forwarding all draw operations to a multisample
  // renderbuffer, which is resolved before any read operations or swaps.
  bool WantExplicitResolve();

  // Whether the WebGL client wants a depth or stencil buffer.
  bool WantDepthOrStencil();

  // Helpers to ensure correct behavior of BlitFramebuffer when using
  // an emulated RGB CHROMIUM_image back buffer.
  bool SetupRGBEmulationForBlitFramebuffer(bool is_user_draw_framebuffer_bound);
  void CleanupRGBEmulationForBlitFramebuffer();

  // Reallocate Multisampled renderbuffer, used by explicit resolve when resize
  // and GPU switch
  bool ReallocateMultisampleRenderbuffer(const IntSize&);

  // Presents swap chain if swap chain is being used and contents have changed.
  void ResolveAndPresentSwapChainIfNeeded();

  // Weak, reset by beginDestruction.
  Client* client_ = nullptr;

  const PreserveDrawingBuffer preserve_drawing_buffer_;
  const WebGLVersion webgl_version_;

  std::unique_ptr<WebGraphicsContext3DProviderWrapper> context_provider_;
  // Lifetime is tied to the m_contextProvider.
  gpu::gles2::GLES2Interface* gl_;
  std::unique_ptr<Extensions3DUtil> extensions_util_;
  IntSize size_ = {-1, -1};
  const bool discard_framebuffer_supported_;
  // Did the user request an alpha channel be allocated.
  const bool want_alpha_channel_;
  // Do we explicitly allocate an alpha channel in our ColorBuffer allocations.
  // Note that this does not apply to |multisample_renderbuffer_|.
  bool allocate_alpha_channel_ = false;
  // Does our allocation have an alpha channel (potentially implicitly created).
  // Note that this determines if |multisample_renderbuffer_| allocates an alpha
  // channel.
  bool have_alpha_channel_ = false;
  const bool premultiplied_alpha_;
  const bool using_gpu_compositing_;
  const bool using_swap_chain_;
  bool has_implicit_stencil_buffer_ = false;

  // The texture target (2D or RECTANGLE) for our allocations.
  GLenum texture_target_ = 0;

  // The current state restorer, which is used to track state dirtying. It is an
  // error to dirty state shared with WebGL while there is no existing state
  // restorer.
  ScopedStateRestorer* state_restorer_ = nullptr;

  // This is used when the user requests either a depth or stencil buffer.
  GLuint depth_stencil_buffer_ = 0;

  // When wantExplicitResolve() returns true, the target of all draw
  // operations.
  GLuint multisample_fbo_ = 0;

  // The id of the renderbuffer storage for |m_multisampleFBO|.
  GLuint multisample_renderbuffer_ = 0;

  // If premultipliedAlpha:false is set during context creation, and a
  // GpuMemoryBuffer is used for the DrawingBuffer's storage, then a separate,
  // regular, OpenGL texture is allocated to hold either the rendering results
  // (if antialias:false) or resolve results (if antialias:true). Then
  // CopyTextureCHROMIUM is used to multiply the alpha channel into the color
  // channels when copying into the GMB.
  GLuint premultiplied_alpha_false_texture_ = 0;

  // A mailbox for the premultiplied_alpha_false_texture_, created lazily if we
  // need to produce it.
  gpu::Mailbox premultiplied_alpha_false_mailbox_;

  // When wantExplicitResolve() returns false, the target of all draw and
  // read operations. When wantExplicitResolve() returns true, the target of
  // all read operations.
  GLuint fbo_ = 0;

  // The ColorBuffer that backs |m_fbo|.
  scoped_refptr<ColorBuffer> back_color_buffer_;

  // The ColorBuffer that was most recently presented to the compositor by
  // PrepareTransferableResourceInternal.
  scoped_refptr<ColorBuffer> front_color_buffer_;

  // True if our contents have been modified since the last presentation of this
  // buffer.
  bool contents_changed_ = true;

  // True if resolveIfNeeded() has been called since the last time
  // markContentsChanged() had been called.
  bool contents_change_resolved_ = false;

  // A bitmask of GL buffer bits (GL_COLOR_BUFFER_BIT,
  // GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT) which need to be
  // auto-cleared.
  GLbitfield buffers_to_auto_clear_ = 0;

  // Whether the client wants a depth or stencil buffer.
  const bool want_depth_;
  const bool want_stencil_;

  // The color space of this buffer's storage, and the color space in which
  // shader samplers will read this buffer.
  const gfx::ColorSpace storage_color_space_;
  const gfx::ColorSpace sampler_color_space_;

  AntialiasingMode anti_aliasing_mode_ = kAntialiasingModeNone;

  bool use_half_float_storage_ = false;

  int max_texture_size_ = 0;
  int sample_count_ = 0;
  int eqaa_storage_sample_count_ = 0;
  bool destruction_in_progress_ = false;
  bool is_hidden_ = false;
  bool has_eqaa_support = false;
  SkFilterQuality filter_quality_ = kLow_SkFilterQuality;

  scoped_refptr<cc::TextureLayer> layer_;

  // Mailboxes that were released by the compositor can be used again by this
  // DrawingBuffer.
  Deque<scoped_refptr<ColorBuffer>> recycled_color_buffer_queue_;

  // If the width and height of the Canvas's backing store don't
  // match those that we were given in the most recent call to
  // reshape(), then we need an intermediate bitmap to read back the
  // frame buffer into. This seems to happen when CSS styles are
  // used to resize the Canvas.
  SkBitmap resizing_bitmap_;

  // In the case of OffscreenCanvas, we do not want to enable the
  // WebGLImageChromium flag, so we replace all the
  // RuntimeEnabledFeatures::WebGLImageChromiumEnabled() call with
  // shouldUseChromiumImage() calls, and set m_chromiumImageUsage to
  // DisallowChromiumImage in the case of OffscreenCanvas.
  ChromiumImageUsage chromium_image_usage_;
  bool ShouldUseChromiumImage();

  bool opengl_flip_y_extension_;

  const gl::GpuPreference initial_gpu_;
  gl::GpuPreference current_active_gpu_;

  base::WeakPtrFactory<DrawingBuffer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DrawingBuffer);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_DRAWING_BUFFER_H_
