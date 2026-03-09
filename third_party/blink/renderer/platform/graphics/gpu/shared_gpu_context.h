// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHARED_GPU_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHARED_GPU_CONTEXT_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_graphics_shared_image_interface_provider.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

class WebGraphicsContext3DProvider;

// SharedGpuContext provides access to a thread-specific GPU context
// that is shared by many callsites throughout the thread.
// When on the main thread, provides access to the same context as
// Platform::CreateSharedOffscreenGraphicsContext3DProvider, and the
// same query as Platform::IsGPUCompositingEnabled().
class PLATFORM_EXPORT SharedGpuContext {
 public:
  // Thread-safe query if gpu compositing is enabled. This should be done before
  // calling ContextProviderWrapper() if the context will be used to make
  // resources meant for the compositor. When it is false, no context will be
  // needed and software-based resources should be given to the compositor
  // instead.
  static bool IsGpuCompositingEnabled();
  // May re-create context if context was lost
  static base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper();
  // Returns an existing context and doesn't create one if none exists.
  static base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  GetExistingContextProviderWrapper();

  // May re-create context provider if context was lost.
  using ContextProviderCallback = base::OnceCallback<void(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>)>;
  static void ContextProviderWrapperAsync(ContextProviderCallback);

  static bool AllowSoftwareToAcceleratedCanvasUpgrade();
  static bool IsValidWithoutRestoringForTesting();

  static WebGraphicsSharedImageInterfaceProvider*
  SharedImageInterfaceProvider();

  // "ImageChromium" refers to putting a canvas into a hardware layer which is
  // directly scanned out of display, bypassing chromium's own GPU composite.
  // It is the same "ImageChromium" referenced by
  // `WebGLImageChromiumEnabled` for example.
  // The name is out of date and refers to the system that morphed into
  // SharedImage.
  // This method performs context-specific check that's not available when
  // RuntimeEnabledFeatures is set.
#if BUILDFLAG(IS_ANDROID)
  static bool MaySupportWebGLImageChromium();
#else
  static bool MaySupportWebGLImageChromium() { return true; }
#endif

  static bool WebGLImageChromiumEnabled();

  // Forces WebGLImageChromiumEnabled() to return the passed-in value.
  // Cleared on the next invocation of Reset() of the global context.
  static void SetWebGLImageChromiumEnabledForTesting(bool enable);

  // Whether mappable SharedImages should be used for canvas2d content with CPU
  // raster.
  static bool UseMappableSharedImagesForCanvas2D();

  // Forces UseMappableSharedImagesForCanvas2D() to return the passed-in value.
  // Cleared on the next invocation of Reset() of the global context.
  static void SetUseMappableSharedImagesForCanvas2DForTesting(bool enable);

  // Whether SharedImages used for canvas2D content should be placed into
  // overlays.
  static bool UseOverlaysForCanvas2D();

  // Whether SharedImages used for canvas2D content that is rasterized according
  // to `raster_mode` may be given usage optimized for low-latency (SCANOUT and
  // CONCURRENT_READ_WRITE).
  static bool LowLatencyUsageSupportedForCanvas2D(RasterMode raster_mode);

  // Forces LowLatencyUsageSupportedForCanvas2D() to return the
  // passed-in value. Cleared on the next invocation of Reset() of the global
  // context.
  static void SetLowLatencyUsageSupportedForCanvas2DForTesting(bool enable);

  using ContextProviderFactory =
      base::RepeatingCallback<std::unique_ptr<WebGraphicsContext3DProvider>()>;
  static void SetContextProviderFactoryForTesting(ContextProviderFactory);
  // Resets the global instance including the |context_provider_factory_| and
  // dropping the context. Should be called at the end of a test that uses this
  // to not interfere with the next test and when terminating web workers.
  static void Reset();

 private:
  friend class base::GenericSequenceLocalStorageSlot<SharedGpuContext>;

  static SharedGpuContext* GetInstanceForCurrentSequence();

  SharedGpuContext();
  void CreateContextProviderIfNeeded(bool only_if_gpu_compositing);
  // Checks if context provider can be created without posting to main thread.
  bool CreateContextProviderIfNeededNoPost(bool only_if_gpu_compositing);
  void CreateSharedImageInterfaceProviderIfNeeded();

  // Can be overridden for tests.
  ContextProviderFactory context_provider_factory_;

  // This is sticky once true, we never need to ask again.
  bool is_gpu_compositing_disabled_ = false;
  std::unique_ptr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;

  std::unique_ptr<WebGraphicsSharedImageInterfaceProvider>
      shared_image_interface_provider_;
};

}  // blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_GPU_SHARED_GPU_CONTEXT_H_
