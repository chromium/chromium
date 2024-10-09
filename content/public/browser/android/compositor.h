// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/slim/layer.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/common/content_export.h"
#include "gpu/ipc/common/surface_handle.h"
#include "ui/android/resources/ui_resource_provider.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc::slim {
class Layer;
}

namespace gpu {
struct SharedMemoryLimits;
}  // namespace gpu

namespace ui {
class ResourceManager;
class UIResourceProvider;
}  // namespace ui

namespace viz {
class ContextProvider;
}

namespace content {
class CompositorClient;

// An interface to the browser-side compositor.
class CONTENT_EXPORT Compositor {
 public:
  virtual ~Compositor() {}

  // Performs the global initialization needed before any compositor
  // instance can be used. This should be called only once.
  static void Initialize();

  // Creates a GL context for the provided |handle|. If a null handle is passed,
  // an offscreen context is created. This must be called on the UI thread.
  using ContextProviderCallback =
      base::OnceCallback<void(scoped_refptr<viz::ContextProvider>)>;
  static void CreateContextProvider(
      gpu::SurfaceHandle handle,
      gpu::SharedMemoryLimits shared_memory_limits,
      ContextProviderCallback callback);

  // Creates and returns a compositor instance.  |root_window| needs to outlive
  // the compositor as it manages callbacks on the compositor.
  static Compositor* Create(CompositorClient* client,
                            gfx::NativeWindow root_window);

  virtual void SetRootWindow(gfx::NativeWindow root_window) = 0;

  // Attaches the layer tree.
  virtual void SetRootLayer(scoped_refptr<cc::slim::Layer> root) = 0;

  // Set the output surface bounds.
  virtual void SetWindowBounds(const gfx::Size& size) = 0;

  // Return the last size set with |SetWindowBounds|.
  virtual const gfx::Size& GetWindowBounds() = 0;

  // Set the output surface which the compositor renders into.
  virtual void SetSurface(
      const base::android::JavaRef<jobject>& surface,
      bool can_be_used_with_surface_control,
      const base::android::JavaRef<jobject>& host_input_token) = 0;

  // Set the background color used by the layer tree host.
  virtual void SetBackgroundColor(int color) = 0;

  // Tells the compositor to allocate an alpha channel.  This won't take effect
  // until the compositor selects a new egl config, usually when the underlying
  // Android Surface changes format.
  virtual void SetRequiresAlphaChannel(bool flag) = 0;

  // Request layout and draw. You only need to call this if you need to trigger
  // Composite *without* having modified the layer tree.
  virtual void SetNeedsComposite() = 0;

  // Returns the UI resource provider associated with the compositor.
  virtual base::WeakPtr<ui::UIResourceProvider> GetUIResourceProvider() = 0;

  // Returns the resource manager associated with the compositor.
  virtual ui::ResourceManager& GetResourceManager() = 0;

  // Caches the back buffer associated with the current surface, if any. The
  // client is responsible for evicting this cache entry before destroying the
  // associated window.
  virtual void CacheBackBufferForCurrentSurface() = 0;

  // Evicts the cache entry created from the cached call above.
  virtual void EvictCachedBackBuffer() = 0;

  // Notifies associated Display to not detach child surface controls during
  // destruction.
  virtual void PreserveChildSurfaceControls() = 0;

  // Registers a callback that is run when the presentation feedback for the
  // next submitted frame is received (it's entirely possible some frames may be
  // dropped between the time this is called and the callback is run).
  // Note that since this might be called on failed presentations, it is
  // deprecated in favor of `RequestSuccessfulPresentationTimeForNextFrame()`
  // which will be called only after a successful presentation.
  using PresentationTimeCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void RequestPresentationTimeForNextFrame(
      PresentationTimeCallback callback) = 0;

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen (it's entirely possible some frames may be dropped between
  // the time this is called and the callback is run).
  using SuccessfulPresentationTimeCallback =
      base::OnceCallback<void(const viz::FrameTimingDetails&)>;
  virtual void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulPresentationTimeCallback callback) = 0;

  // Control whether `CompositorClient::DidSwapBuffers` should be called. The
  // default is false. Note this is asynchronous. Any pending callbacks may
  // immediately after enabling may still be missed; best way to avoid this is
  // to call this before calling `SetNeedsComposite`. Also there may be trailing
  // calls to `DidSwapBuffers` after unsetting this.
  virtual void SetDidSwapBuffersCallbackEnabled(bool enable) = 0;

 protected:
  Compositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_COMPOSITOR_H_
