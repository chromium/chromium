// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
#define DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "device/vr/openxr/openxr_composition_layer.h"
#include "device/vr/openxr/openxr_layers.h"
#include "device/vr/openxr/openxr_swapchain_info.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "third_party/openxr/src/include/openxr/openxr.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class GpuFence;
}  // namespace gfx

namespace gpu {
class SharedImageInterface;
}  // namespace gpu

namespace viz {
class ContextProvider;
}  // namespace viz

namespace device {
class OpenXrCompositionLayer;
class OpenXrExtensionEnumeration;
class OpenXrViewConfiguration;

// This class exists to provide an abstraction for the different rendering
// paths that can be taken by OpenXR (e.g. DirectX vs. GLES). Any OpenXr methods
// that need types specific for a given renderer type should go through this
// interface.
class OpenXrGraphicsBinding {
 public:
  // Gets the set of RequiredExtensions that need to be present on the platform.
  static void GetRequiredExtensions(std::vector<const char*>& extensions);

  // Gets any OptionalExtensions that should be enabled if present.
  static std::vector<std::string> GetOptionalExtensions();

  virtual ~OpenXrGraphicsBinding();

  // Ensures that the GraphicsBinding is ready for use.
  virtual bool Initialize(XrInstance instance, XrSystemId system) = 0;

  // Called after the XrSession has been created.
  void OnSessionCreated(XrSpace local_space, bool is_webgpu);

  // Called when the XrSession is going to destroyed.
  void OnSessionDestroyed(gpu::SharedImageInterface* sii);

  // Gets a pointer to a platform-specific XrGraphicsBindingFoo. The pointer is
  // guaranteed to live as long as this class does.
  virtual const void* GetSessionCreateInfo() const = 0;

  // Gets the format that we expect from the platform swapchain.
  virtual int64_t GetSwapchainFormat(XrSession session) const = 0;

  // Calls xrEnumerateSwapChain and updates the stored OpenXrSwapchainInfo
  // available via `GetSwapChainImages`.
  virtual XrResult EnumerateSwapchainImages(OpenXrCompositionLayer& layer) = 0;

  // Returns whether or not the platform believes it can support using Shared
  // buffers/images.
  virtual bool CanUseSharedImages() const = 0;

  // Called when a frame is going to end without any attempt at rendering, in
  // case there is any early cleanup to do that would otherwise occur during
  // `Render`.
  virtual void CleanupWithoutSubmit() = 0;

  // Returns the maximum texture size allowed to be created with the current
  // graphics binding. Textures larger than this size may be truncated during
  // cross-process transportation of the textures and result in one viewport
  // being rendered on over half of the texture, which can lead to uncomfortable
  // rendering artifacts.
  virtual gfx::Size GetMaxTextureSize() = 0;

  // Called to indicate which of Overlay and WebXR content is expected to be
  // composited during calls to `Render`.
  void SetOverlayAndWebXrVisibility(bool overlay_visible, bool webxr_visible);

  // There are three different paths that submitting an image can take. In two
  // of them, we provide the surface/image for the page to draw into. The third
  // is only supported on Windows or via the overlay code and requires
  // submitting a texture handle to us, which we don't own. The first two
  // rendering methods will have their data tied to the active swapchain image,
  // but for the third method, we don't have to do any lifecycle management and
  // will just hold a reference to the latest submitted texture. It will be
  // valid until we end the frame, but can then be overwritten independently
  // during the cycle. Since this third code-path only exists on Windows we
  // restrict this method to that platform.
#if BUILDFLAG(IS_WIN)
  virtual void SetWebXrTexture(mojo::PlatformHandle texture_handle,
                               const gpu::SyncToken& sync_token,
                               const gfx::RectF& left,
                               const gfx::RectF& right) = 0;
#endif

  // Much like the `SetWebXrTexture` path above, the texture submitted here is
  // owned by the browser process with corresponding lifetime management
  // and synchronization happening there. It's valid until we tell it we're done
  // with the texture, but it's not tied to a swapchain info the same way that
  // the page's textures are, so we provide this additional method and simply
  // overwrite the overlay whenever we receive it.
  virtual bool SetOverlayTexture(gfx::GpuMemoryBufferHandle texture,
                                 const gpu::SyncToken& sync_token,
                                 const gfx::RectF& left,
                                 const gfx::RectF& right) = 0;

  // Will be called when SetSwapchainImageSize is called, even if a change is
  // not made, to allow child classes/concrete implementations to override any
  // state that they may need to override as a result of the swapchain image
  // size changing. Note that the caller is responsible for recreating the
  // swapchain in response to this call, but it likely has not been recreated
  // yet.
  virtual void OnSwapchainImageSizeChanged(OpenXrCompositionLayer& layer) {}

  // Called at the end of ActivateSwapchainImage. Allows Children to setup the
  // appropriate image to be rendered to by, e.g. Render calls, if that needs
  // to happen ahead of time.
  virtual void OnSwapchainImageActivated(OpenXrCompositionLayer& layer,
                                         gpu::SharedImageInterface* sii) = 0;

  // Return if the graphics binding supports multiple XR layers.
  virtual bool SupportsLayers() const = 0;

  // Resizes the shared buffer for the given swapchain info if the transfer size
  // has changed.
  virtual void ResizeSharedBuffer(OpenXrCompositionLayer& layer,
                                  OpenXrSwapchainInfo& swap_chain_info,
                                  gpu::SharedImageInterface* sii) = 0;

  // Called to indicate which graphics API produced the textures submitted to
  // OpenXR. Does not affect the API used for compositing.
  bool IsWebGPUSession() const { return webgpu_session_; }

  // If the layer should be flipped, return a pointer to the
  // XrCompositionLayerImageLayoutFB. Otherwise, return null. The return value
  // should be set to the "next" field of the XrCompositionLayer* struct.
  const void* GetFlipLayerLayout() const;

  // We check if the base layer is using shared images.
  bool IsUsingSharedImages() const;

  // Called when context proivder is lost.
  void OnContextProviderLost();

  // Build an OpenXrLayers object that provides data needed for xrEndFrame
  // (e.g. a list of XrCompositionLayerBaseHeader).
  std::unique_ptr<OpenXrLayers> GetLayersForViewConfig(
      OpenXrApiWrapper* openxr,
      const OpenXrViewConfiguration& view_config) const;

  // A few methods that only operate on the base layer.

  // Create the XrSwapchain and swapchain images for the base layer.
  XrResult CreateBaseLayerSwapchain(XrSession session, uint32_t sample_count);

  // Clears the list of images allocated during `CreateBaseLayerSwapchain` and
  // if a context_provider is provided and the Swapchain entries have had
  // corresponding SharedImages created via `CreateBaseLayerSharedImages` will
  // also clean up those SharedImages.
  void DestroyBaseLayerSwapchain(gpu::SharedImageInterface* sii);

  // Creates SharedImages for (and thus populates the mailbox holders of) all
  // currently held OpenXrSwapchainInfo objects.
  void CreateBaseLayerSharedImages(gpu::SharedImageInterface* sii);

  // Returns the previously set swapchain image size, or 0,0 if one is not set.
  gfx::Size GetProjectionLayerSwapchainImageSize();

  // Sets the size of the swapchain images being used by the system. Does *not*
  // cause a corresponding re-creation of the Swapchain or Shared Images; which
  // should be driven by the caller. If a transfer size has not been specified
  // yet, will also set the transfer size as well.
  void SetProjectionLayerSwapchainImageSize(
      const gfx::Size& swapchain_image_size);

  // Return if the XrSwapchain is available.
  bool HasBaseLayerColorSwapchain() const;

  // Sets the size of the texture being used between the renderer process and
  // our process. This is largely driven by the page and any framebuffer scaling
  // it may apply. When rendering to the SwapchainImage scaling will be
  // performed as necessary.
  void SetProjectionLayerTransferSize(const gfx::Size& transfer_size);

  // Performs a server wait on the provided gpu_fence. Returns true if it was
  // able to successfully schedule and perform the wait, and false otherwise.
  bool WaitOnBaseLayerFence(gfx::GpuFence& gpu_fence);

  // Updates the active swapchain image size if the transfer size has changed.
  // No-ops if there is currently no active swapchain image.
  void UpdateProjectionLayerActiveSwapchainImageSize(
      gpu::SharedImageInterface* sii);

  // Build XR projection views for the base layer.
  std::vector<XrCompositionLayerProjectionView> GetBaseLayerProjectionViews(
      const OpenXrViewConfiguration& view_config) const;

  // A few methods that operate on all layers.

  // Acquire and activate swapchain images from the OpenXr system
  XrResult ActivateSwapchainImages(gpu::SharedImageInterface* sii);

  // Release the active swapchain images from the OpenXr system. This is called
  // before calling EndFrame and will enable acquiring a new swapchain image for
  // the next frame.
  XrResult ReleaseActiveSwapchainImages();

  // Populate the shared image data in XRFrameData.
  void PopulateSharedImageData(mojom::XRFrameData& frame_data);

  // Causes the GraphicsBinding to render the currently active swapchain image.
  bool Render(const scoped_refptr<viz::ContextProvider>& context_provider,
              const std::vector<LayerId>& updated_layers);

  // Create a composition layer. The id is given in layer_data.
  bool CreateCompositionLayer(
      mojom::XRCompositionLayerDataPtr layer_data,
      gpu::SharedImageInterface* shared_image_interface);

  // Get a composition layer by its layer id. Returns nullptr
  // if the layer id doesn't exist.
  OpenXrCompositionLayer* GetCompositionLayer(LayerId layer_id);

  // Destroy a composition layer.
  void DestroyCompositionLayer(LayerId layer_id,
                               gpu::SharedImageInterface* sii);

  // Specify the layers that should be rendered and should have shared
  // images available.
  void SetEnabledCompositionLayers(const std::vector<LayerId>& layer_ids,
                                   XrSession session,
                                   uint32_t swapchain_sample_count,
                                   gpu::SharedImageInterface* sii);

 protected:
  explicit OpenXrGraphicsBinding(
      const OpenXrExtensionEnumeration* extension_enum);

  bool ShouldRenderBaseLayer() const;

  // Performs a server wait on the provided gpu_fence. Returns true if it was
  // able to successfully schedule and perform the wait, and false otherwise.
  virtual bool WaitOnFence(OpenXrCompositionLayer& layer,
                           gfx::GpuFence& gpu_fence) = 0;

  // Render a single layer.
  virtual bool RenderLayer(
      OpenXrCompositionLayer& layer,
      const scoped_refptr<viz::ContextProvider>& context_provider) = 0;

  // Creates SharedImages for (and thus populates the mailbox holders of) all
  // currently held OpenXrSwapchainInfo objects.
  virtual void CreateSharedImages(OpenXrCompositionLayer& layer,
                                  gpu::SharedImageInterface* sii) = 0;

  // Indicates whether the graphics binding expects the submitted image to need
  // to be flipped when being submitted to the runtime.
  virtual bool ShouldFlipSubmittedImage(
      OpenXrCompositionLayer& layer) const = 0;

  // Create a graphics binding specific data.
  virtual std::unique_ptr<OpenXrCompositionLayer::GraphicsBindingData>
  CreateLayerGraphicsBindingData() const = 0;

  // Called when SetOverlayAndWebXrVisibility is called and the internal flags
  // have been updated.
  virtual void OnSetOverlayAndWebXrVisibility() {}

  // Build XR projection views for a projection layer.
  std::vector<XrCompositionLayerProjectionView> GetProjectionViews(
      const OpenXrViewConfiguration& view_config,
      OpenXrCompositionLayer& layer) const;

  std::unique_ptr<OpenXrCompositionLayer> base_layer_;
  // Each client created layer has a unique ID.
  std::map<LayerId, std::unique_ptr<OpenXrCompositionLayer>> layers_;
  // This sequence defines which layers should be composed.
  std::vector<LayerId> layers_sequence_;
  bool has_custom_projection_layer_ = false;
  bool webgpu_session_ = false;
  bool fb_composition_layer_ext_enabled_ = false;
  bool webxr_visible_ = true;
  bool overlay_visible_ = false;

  // This will only be valid if `fb_composition_layer_ext_enabled_` is true.
  XrCompositionLayerImageLayoutFB y_flip_layer_layout_;
};

}  // namespace device

#endif  // DEVICE_VR_OPENXR_OPENXR_GRAPHICS_BINDING_H_
