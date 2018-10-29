// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/service/direct_composition_surface_win.h"

#include <d3d11_1.h>
#include <dcomptypes.h>
#include <dxgi1_6.h>

#include "base/containers/circular_deque.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/ipc/service/direct_composition_child_surface_win.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_win.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/transform.h"
#include "ui/gl/dc_renderer_layer_params.h"
#include "ui/gl/egl_util.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_image_dxgi.h"
#include "ui/gl/gl_image_memory.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_surface_presentation_helper.h"

#ifndef EGL_ANGLE_flexible_surface_compatibility
#define EGL_ANGLE_flexible_surface_compatibility 1
#define EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE 0x33A6
#endif /* EGL_ANGLE_flexible_surface_compatibility */

namespace gpu {
namespace {
// Number of stable frames before the swap chain can be resized
static constexpr int kNumFramesBeforeSwapChainResize = 30;

// Some drivers fail to correctly handle BT.709 video in overlays. This flag
// converts them to BT.601 in the video processor.
const base::Feature kFallbackBT709VideoToBT601{
    "FallbackBT709VideoToBT601", base::FEATURE_DISABLED_BY_DEFAULT};

bool SizeContains(const gfx::Size& a, const gfx::Size& b) {
  return gfx::Rect(a).Contains(gfx::Rect(b));
}

// This keeps track of whether the previous 30 frames used Overlays or GPU
// composition to present.
class PresentationHistory {
 public:
  static const int kPresentsToStore = 30;

  PresentationHistory() {}

  void AddSample(DXGI_FRAME_PRESENTATION_MODE mode) {
    if (mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
      composed_count_++;

    presents_.push_back(mode);
    if (presents_.size() > kPresentsToStore) {
      DXGI_FRAME_PRESENTATION_MODE first_mode = presents_.front();
      if (first_mode == DXGI_FRAME_PRESENTATION_MODE_COMPOSED)
        composed_count_--;
      presents_.pop_front();
    }
  }

  bool valid() const { return presents_.size() >= kPresentsToStore; }
  int composed_count() const { return composed_count_; }

 private:
  base::circular_deque<DXGI_FRAME_PRESENTATION_MODE> presents_;
  int composed_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PresentationHistory);
};

class ScopedReleaseKeyedMutex {
 public:
  ScopedReleaseKeyedMutex(Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
                          UINT64 key)
      : keyed_mutex_(keyed_mutex), key_(key) {
    DCHECK(keyed_mutex);
  }

  ~ScopedReleaseKeyedMutex() {
    HRESULT hr = keyed_mutex_->ReleaseSync(key_);
    DCHECK(SUCCEEDED(hr));
  }

 private:
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex_;
  UINT64 key_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScopedReleaseKeyedMutex);
};

struct OverlaySupportInfo {
  OverlayFormat overlay_format;
  DXGI_FORMAT dxgi_format;
  UINT flags;
};

// Indicates if overlay support has been initialized.
bool g_overlay_support_initialized = false;

// Indicates support for either NV12 or YUY2 hardware overlays.
bool g_supports_overlays = false;

// Indicates support for hardware overlay scaling.
bool g_supports_scaled_overlays = true;

// Used for workaround limiting overlay size to monitor size.
gfx::Size g_overlay_monitor_size;

// Overridden when NV12 is supported, and kDirectCompositionPreferNV12Overlays
// finch feature is enabled. Default value is set to YUY2 so that we use a valid
// format for swap chains when forced to enable overlay code path but hardware
// overlays are not supported.
OverlayFormat g_overlay_format_used = OverlayFormat::kYUY2;
DXGI_FORMAT g_overlay_dxgi_format_used = DXGI_FORMAT_YUY2;

// This is the raw support info, which shouldn't depend on field trial state, or
// command line flags. Ordered by most preferred to least preferred format.
OverlaySupportInfo g_overlay_support_info[] = {
    {OverlayFormat::kNV12, DXGI_FORMAT_NV12, 0},
    {OverlayFormat::kYUY2, DXGI_FORMAT_YUY2, 0},
    {OverlayFormat::kBGRA, DXGI_FORMAT_B8G8R8A8_UNORM, 0},
};

bool IsDirectCompositionSupported() {
  static bool initialized = false;
  static bool supported = false;
  if (initialized)
    return supported;
  initialized = true;

  // Before Windows 10 Anniversary Update (Redstone 1), overlay planes wouldn't
  // be assigned to non-UWP apps.
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return false;

  // Blacklist direct composition if MCTU.dll or MCTUX.dll are injected. These
  // are user mode drivers for display adapters from Magic Control Technology
  // Corporation.
  if (GetModuleHandle(TEXT("MCTU.dll")) || GetModuleHandle(TEXT("MCTUX.dll"))) {
    DLOG(ERROR) << "Blacklisted due to third party modules";
    return false;
  }

  // Flexible surface compatibility is required to be able to MakeCurrent with
  // the default pbuffer surface.
  if (!gl::GLSurfaceEGL::IsEGLFlexibleSurfaceCompatibilitySupported()) {
    DLOG(ERROR) << "EGL_ANGLE_flexible_surface_compatibility not supported";
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return false;
  }

  // This will fail if DirectComposition DLL can't be loaded.
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device =
      gl::QueryDirectCompositionDevice(d3d11_device);
  if (!dcomp_device) {
    DLOG(ERROR) << "Failed to retrieve direct composition device";
    return false;
  }

  // This will fail if the D3D device is "Microsoft Basic Display Adapter".
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device;
  if (FAILED(d3d11_device.CopyTo(video_device.GetAddressOf()))) {
    DLOG(ERROR) << "Failed to retrieve video device";
    return false;
  }

  supported = true;
  return true;
}

void InitializeHardwareOverlaySupport() {
  if (g_overlay_support_initialized)
    return;
  g_overlay_support_initialized = true;

  if (!IsDirectCompositionSupported())
    return;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  if (FAILED(d3d11_device.CopyTo(dxgi_device.GetAddressOf()))) {
    DLOG(ERROR) << "Failed to retrieve DXGI device";
    return;
  }

  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  if (FAILED(dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf()))) {
    DLOG(ERROR) << "Failed to retrieve DXGI adapter";
    return;
  }

  bool supports_nv12_rec709 = false;
  unsigned int i = 0;
  while (true) {
    Microsoft::WRL::ComPtr<IDXGIOutput> output;
    if (FAILED(dxgi_adapter->EnumOutputs(i++, output.GetAddressOf())))
      break;
    DCHECK(output);
    Microsoft::WRL::ComPtr<IDXGIOutput3> output3;
    if (FAILED(output.CopyTo(output3.GetAddressOf())))
      continue;
    DCHECK(output3);

    for (auto& info : g_overlay_support_info) {
      if (FAILED(output3->CheckOverlaySupport(
              info.dxgi_format, d3d11_device.Get(), &info.flags))) {
        continue;
      }
      // Per Intel's request, use NV12 only when
      // COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 is also supported. Rec 709 is
      // commonly used for H.264 and HEVC. At least one Intel Gen9 SKU will not
      // support NV12 overlays.
      if (info.overlay_format == OverlayFormat::kNV12) {
        UINT color_space_support_flags = 0;
        Microsoft::WRL::ComPtr<IDXGIOutput4> output4;
        if (FAILED(output.CopyTo(output4.GetAddressOf())))
          continue;

        if (FAILED(output4->CheckOverlayColorSpaceSupport(
                info.dxgi_format, DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709,
                d3d11_device.Get(), &color_space_support_flags))) {
          continue;
        }
        supports_nv12_rec709 =
            !!(color_space_support_flags &
               DXGI_OVERLAY_COLOR_SPACE_SUPPORT_FLAG_PRESENT);
      }

      // Formats are ordered by most preferred to least preferred. Don't choose
      // a less preferred format, but keep going so that we can record overlay
      // support for all formats in UMA.
      if (g_supports_overlays)
        continue;
      // Don't use BGRA overlays in any case, but record support in UMA.
      if (info.overlay_format == OverlayFormat::kBGRA)
        continue;
      // Overlays are supported for NV12 only if the feature flag to prefer NV12
      // over YUY2 is enabled.
      bool prefer_nv12 = base::FeatureList::IsEnabled(
          features::kDirectCompositionPreferNV12Overlays);
      if (info.overlay_format == OverlayFormat::kNV12 &&
          (!prefer_nv12 || !supports_nv12_rec709))
        continue;
      // Some new Intel drivers only claim to support unscaled overlays, but
      // scaled overlays still work. It's possible DWM works around it by
      // performing an extra scaling Blt before calling the driver. Even when
      // scaled overlays aren't actually supported, presentation using the
      // overlay path should be relatively efficient.
      if (info.flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                        DXGI_OVERLAY_SUPPORT_FLAG_SCALING)) {
        g_overlay_format_used = info.overlay_format;
        g_overlay_dxgi_format_used = info.dxgi_format;

        g_supports_overlays = true;
        g_supports_scaled_overlays =
            !!(info.flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);

        DXGI_OUTPUT_DESC monitor_desc = {};
        if (SUCCEEDED(output3->GetDesc(&monitor_desc))) {
          g_overlay_monitor_size =
              gfx::Rect(monitor_desc.DesktopCoordinates).size();
        }
      }
    }
    // Early out after the first output that reports overlay support. All
    // outputs are expected to report the same overlay support according to
    // Microsoft's WDDM documentation:
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/display/multiplane-overlay-hardware-requirements
    // TODO(sunnyps): If the above is true, then we can only look at first
    // output instead of iterating over all outputs.
    if (g_supports_overlays)
      break;
  }
  for (const auto& info : g_overlay_support_info) {
    const std::string kOverlaySupportFlagsUmaPrefix =
        "GPU.DirectComposition.OverlaySupportFlags2.";
    base::UmaHistogramSparse(kOverlaySupportFlagsUmaPrefix +
                                 OverlayFormatToString(info.overlay_format),
                             info.flags);
    if ((info.overlay_format == OverlayFormat::kNV12) &&
        (info.flags & (DXGI_OVERLAY_SUPPORT_FLAG_DIRECT |
                       DXGI_OVERLAY_SUPPORT_FLAG_SCALING))) {
      // Recorded only when NV12 is supported
      UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlayNV12Rec709Supported",
                            supports_nv12_rec709);
    }
  }
  if (g_supports_overlays) {
    UMA_HISTOGRAM_ENUMERATION("GPU.DirectComposition.OverlayFormatUsed2",
                              g_overlay_format_used);
  }
  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.OverlaysSupported",
                        g_supports_overlays);
}

bool CreateSurfaceHandleHelper(HANDLE* handle) {
  using PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE =
      HRESULT(WINAPI*)(DWORD, SECURITY_ATTRIBUTES*, HANDLE*);
  static PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE create_surface_handle_function =
      nullptr;

  if (!create_surface_handle_function) {
    HMODULE dcomp = ::GetModuleHandleA("dcomp.dll");
    if (!dcomp) {
      DLOG(ERROR) << "Failed to get handle for dcomp.dll";
      return false;
    }
    create_surface_handle_function =
        reinterpret_cast<PFN_DCOMPOSITION_CREATE_SURFACE_HANDLE>(
            ::GetProcAddress(dcomp, "DCompositionCreateSurfaceHandle"));
    if (!create_surface_handle_function) {
      DLOG(ERROR)
          << "Failed to get address for DCompositionCreateSurfaceHandle";
      return false;
    }
  }

  HRESULT hr = create_surface_handle_function(COMPOSITIONOBJECT_ALL_ACCESS,
                                              nullptr, handle);
  if (FAILED(hr)) {
    DLOG(ERROR) << "DCompositionCreateSurfaceHandle failed with error "
                << std::hex << hr;
    return false;
  }

  return true;
}
}  // namespace

// DCLayerTree manages a tree of direct composition visuals, and associated
// swap chains for given overlay layers.  It maintains a list of pending layers
// submitted using ScheduleDCLayer() that are presented and committed in
// CommitAndClearPendingOverlays().
class DCLayerTree {
 public:
  DCLayerTree(const GpuDriverBugWorkarounds& workarounds)
      : workarounds_(workarounds) {}

  // Returns true on success.
  bool Initialize(HWND window,
                  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device);

  // Information about the back buffer surface.  Cached on every frame to check
  // if back buffer changed from previous frame.  Back buffer uses either a swap
  // chain, or a direct composition surface depending on whether layers are used
  // or not (see DirectCompositionSurfaceChildWin).
  struct BackbufferInfo {
    // Set if backbuffer is using a swap chain currently.
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain;

    // Set if backbuffer is using a direct composition surface currently.
    Microsoft::WRL::ComPtr<IDCompositionSurface> dcomp_surface;
    uint64_t dcomp_surface_serial;

    bool operator!=(const BackbufferInfo& other) const {
      return std::tie(swap_chain, dcomp_surface, dcomp_surface_serial) !=
             std::tie(other.swap_chain, other.dcomp_surface,
                      other.dcomp_surface_serial);
    }
  };
  // Present pending overlay layers, and perform a direct composition commit if
  // necessary using provided backbuffer info for creating backbuffer visual.
  // Returns true if presentation and commit succeeded.
  bool CommitAndClearPendingOverlays(BackbufferInfo backbuffer_info);

  // Schedule an overlay layer for the next CommitAndClearPendingOverlays call.
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params);

  // Called by SwapChainPresenter to initialize video processor that can handle
  // at least given input and output size.  The video processor is shared across
  // layers so the same one can be reused if it's large enough.  Returns true on
  // success.
  bool InitializeVideoProcessor(const gfx::Size& input_size,
                                const gfx::Size& output_size);

  const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor() const {
    return video_processor_;
  }

  const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>&
  video_processor_enumerator() const {
    return video_processor_enumerator_;
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain1> GetLayerSwapChainForTesting(
      size_t index) const;

  const GpuDriverBugWorkarounds& workarounds() const { return workarounds_; }

 private:
  class SwapChainPresenter;

  // Update backbuffer visual with provided info.  Returns true if the visual
  // changed, and a direct composition commit is needed.
  bool UpdateBackbufferVisual(BackbufferInfo info);

  const GpuDriverBugWorkarounds workarounds_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDCompositionTarget> dcomp_target_;

  // The video processor is cached so SwapChains don't have to recreate it
  // whenever they're created.
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;

  // Current video processor input and output size.
  gfx::Size video_input_size_;
  gfx::Size video_output_size_;

  // Direct composition visual for window.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> root_visual_;

  // Direct composition for backbuffer surface.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> backbuffer_visual_;

  // Cached backbuffer info for last frame.
  BackbufferInfo backbuffer_info_;

  // List of pending overlay layers from ScheduleDCLayer().
  std::vector<std::unique_ptr<ui::DCRendererLayerParams>> pending_overlays_;

  // List of swap chain presenters for previous frame.
  std::vector<std::unique_ptr<SwapChainPresenter>> video_swap_chains_;

  DISALLOW_COPY_AND_ASSIGN(DCLayerTree);
};

// SwapChainPresenter holds a swap chain, direct composition visuals, and other
// associated resources for a single overlay layer.  It is updated by calling
// PresentToSwapChain(), and can update or recreate resources as necessary.
class DCLayerTree::SwapChainPresenter {
 public:
  SwapChainPresenter(DCLayerTree* layer_tree,
                     Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
                     Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device,
                     Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
                     Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context);
  ~SwapChainPresenter();

  // Present the given overlay to swap chain.  |needs_commit| is true if direct
  // composition visuals changed, and a commit is needed.  Returns true on
  // success.
  bool PresentToSwapChain(const ui::DCRendererLayerParams& overlay,
                          bool* needs_commit);

  const Microsoft::WRL::ComPtr<IDXGISwapChain1>& swap_chain() const {
    return swap_chain_;
  }

  const Microsoft::WRL::ComPtr<IDCompositionVisual2>& visual() const {
    return clip_visual_;
  }

 private:
  // Upload given YUV buffers to NV12 |staging_texture_|.  Returns true on
  // success.
  bool UploadVideoImages(gl::GLImageMemory* y_image_memory,
                         gl::GLImageMemory* uv_image_memory);

  // Initialize video processor to handle at least the given input and output
  // size.  Can reuse existing video processor if it's large enough.  Returns
  // true on success.
  bool InitializeVideoProcessor(const gfx::Size& in_size,
                                const gfx::Size& out_size);

  // Recreate swap chain using given size.  Use preferred YUV format if |yuv| is
  // true, or BGRA otherwise.  Set protected video flags if |protected_video| is
  // true. Returns true on success.
  bool ReallocateSwapChain(const gfx::Size& swap_chain_size,
                           bool use_yuv_swap_chain,
                           bool protected_video);

  // Returns true if YUV swap chain should be preferred over BGRA swap chain.
  // This changes over time based on stats recorded in |presentation_history|.
  bool ShouldUseYUVSwapChain();

  // Perform a blit using video processor from given input texture to swap chain
  // backbuffer. |input_texture| is the input texture (array), and |input_level|
  // is the index of the texture in the texture array.  |keyed_mutex| is
  // optional, and is used to lock the resource for reading.  |input_size| is
  // the size of the input texture, and |src_color_space| is the color space
  // of the video.
  bool VideoProcessorBlt(Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
                         UINT input_level,
                         Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
                         const gfx::Size& input_size,
                         const gfx::ColorSpace& src_color_space);

  // Returns optimal swap chain size for given layer.
  gfx::Size CalculateSwapChainSize(const ui::DCRendererLayerParams& params);

  // Returns a stable swap chain size to prevent frequently recreating the swap
  // chain until a number of frames with the size have been presented.
  gfx::Size GetStableSwapChainSize(gfx::Size requested_swap_chain_size);

  // Update direct composition visuals for layer with given swap chain size, and
  // returns true if a commit is needed.
  bool UpdateVisuals(const ui::DCRendererLayerParams& params,
                     const gfx::Size& swap_chain_size);

  // Layer tree instance that owns this swap chain presenter.
  DCLayerTree* layer_tree_;

  // Current size of swap chain.
  gfx::Size swap_chain_size_;

  // Current minimum input and output size of the video processor.
  gfx::Size processor_input_size_;
  gfx::Size processor_output_size_;

  // Whether the current swap chain is using the preferred YUV format.
  bool is_yuv_swapchain_ = false;

  // Whether the current swap chain is presenting protected video.
  bool is_protected_video_ = false;

  // Presentation history to track if swap chain was composited or used hardware
  // overlays.
  PresentationHistory presentation_history_;

  // Whether creating a YUV swap chain failed.
  bool failed_to_create_yuv_swapchain_ = false;

  // Number of frames since we switched from YUV to BGRA swap chain, or
  // vice-versa.
  int frames_since_color_space_change_ = 0;
  int frames_since_stable_swap_chain_resize_ = 0;
  gfx::Size last_requested_swap_chain_size_;

  // This struct is used to cache information about what visuals are currently
  // being presented so that properties that aren't changed aren't sent to
  // DirectComposition.
  struct VisualInfo {
    gfx::Point offset;
    gfx::Transform transform;
    bool is_clipped = false;
    gfx::Rect clip_rect;
  } visual_info_;

  // Direct composition visual containing the swap chain content.  Child of
  // |clip_visual_|.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> content_visual_;

  // Direct composition visual that applies the clip rect.  Parent of
  // |content_visual_|, and root of the visual tree for this layer.
  Microsoft::WRL::ComPtr<IDCompositionVisual2> clip_visual_;

  // These are the GLImages that were presented in the last frame.
  std::vector<scoped_refptr<gl::GLImage>> last_gl_images_;

  // NV12 staging texture used for software decoded YUV buffers.  Mapped to CPU
  // for copying from YUV buffers.
  Microsoft::WRL::ComPtr<ID3D11Texture2D> staging_texture_;
  gfx::Size staging_texture_size_;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device_;
  Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device_;
  Microsoft::WRL::ComPtr<IDXGISwapChain1> swap_chain_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> out_view_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessor> video_processor_;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>
      video_processor_enumerator_;
  Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device_;
  Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context_;

  // Handle returned by DCompositionCreateSurfaceHandle() used to create swap
  // chain that can be used for direct composition.
  base::win::ScopedHandle swap_chain_handle_;

  DISALLOW_COPY_AND_ASSIGN(SwapChainPresenter);
};

bool DCLayerTree::Initialize(
    HWND window,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device) {
  DCHECK(d3d11_device);
  d3d11_device_ = std::move(d3d11_device);
  DCHECK(dcomp_device);
  dcomp_device_ = std::move(dcomp_device);

  // This can fail if the D3D device is "Microsoft Basic Display Adapter".
  if (FAILED(d3d11_device_.CopyTo(video_device_.GetAddressOf()))) {
    DLOG(ERROR) << "Failed to retrieve video device from D3D11 device";
    return false;
  }
  DCHECK(video_device_);

  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  DCHECK(context);
  context.CopyTo(video_context_.GetAddressOf());
  DCHECK(video_context_);

  Microsoft::WRL::ComPtr<IDCompositionDesktopDevice> desktop_device;
  dcomp_device_.CopyTo(desktop_device.GetAddressOf());
  DCHECK(desktop_device);

  HRESULT hr = desktop_device->CreateTargetForHwnd(
      window, TRUE, dcomp_target_.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTargetForHwnd failed with error " << std::hex << hr;
    return false;
  }

  dcomp_device_->CreateVisual(root_visual_.GetAddressOf());
  DCHECK(root_visual_);
  dcomp_target_->SetRoot(root_visual_.Get());

  return true;
}

bool DCLayerTree::InitializeVideoProcessor(const gfx::Size& input_size,
                                           const gfx::Size& output_size) {
  if (video_processor_ && SizeContains(video_input_size_, input_size) &&
      SizeContains(video_output_size_, output_size))
    return true;
  video_input_size_ = input_size;
  video_output_size_ = output_size;

  video_processor_.Reset();
  video_processor_enumerator_.Reset();
  D3D11_VIDEO_PROCESSOR_CONTENT_DESC desc = {};
  desc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
  desc.InputFrameRate.Numerator = 60;
  desc.InputFrameRate.Denominator = 1;
  desc.InputWidth = input_size.width();
  desc.InputHeight = input_size.height();
  desc.OutputFrameRate.Numerator = 60;
  desc.OutputFrameRate.Denominator = 1;
  desc.OutputWidth = output_size.width();
  desc.OutputHeight = output_size.height();
  desc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
  HRESULT hr = video_device_->CreateVideoProcessorEnumerator(
      &desc, video_processor_enumerator_.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessorEnumerator failed with error "
                << std::hex << hr;
    return false;
  }

  hr = video_device_->CreateVideoProcessor(video_processor_enumerator_.Get(), 0,
                                           video_processor_.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateVideoProcessor failed with error " << std::hex << hr;
    return false;
  }

  // Auto stream processing (the default) can hurt power consumption.
  video_context_->VideoProcessorSetStreamAutoProcessingMode(
      video_processor_.Get(), 0, FALSE);
  return true;
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DCLayerTree::GetLayerSwapChainForTesting(size_t index) const {
  if (index < video_swap_chains_.size())
    return video_swap_chains_[index]->swap_chain();
  return nullptr;
}

DCLayerTree::SwapChainPresenter::SwapChainPresenter(
    DCLayerTree* layer_tree,
    Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device,
    Microsoft::WRL::ComPtr<IDCompositionDevice2> dcomp_device,
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device,
    Microsoft::WRL::ComPtr<ID3D11VideoContext> video_context)
    : layer_tree_(layer_tree),
      d3d11_device_(d3d11_device),
      dcomp_device_(dcomp_device),
      video_device_(video_device),
      video_context_(video_context) {}

DCLayerTree::SwapChainPresenter::~SwapChainPresenter() {}

bool DCLayerTree::SwapChainPresenter::ShouldUseYUVSwapChain() {
  // Always prefer YUV swap chain for protected video for now.
  // TODO(crbug.com/850799): Assess power/perf impact when protected video
  // swap chain is composited by DWM.
  if (is_protected_video_)
    return true;

  // Start out as YUV.
  if (!presentation_history_.valid())
    return true;
  int composition_count = presentation_history_.composed_count();

  // It's more efficient to use a BGRA backbuffer instead of YUV if overlays
  // aren't being used, as otherwise DWM will use the video processor a second
  // time to convert it to BGRA before displaying it on screen.

  if (is_yuv_swapchain_) {
    // Switch to BGRA once 3/4 of presents are composed.
    return composition_count < (PresentationHistory::kPresentsToStore * 3 / 4);
  } else {
    // Switch to YUV once 3/4 are using overlays (or unknown).
    return composition_count < (PresentationHistory::kPresentsToStore / 4);
  }
}

// To avoid frequent swap chain recreation during video resizing, keep
// the same swap chain size until the onscreen video size is stable for
// at least 30 frames.
gfx::Size DCLayerTree::SwapChainPresenter::GetStableSwapChainSize(
    gfx::Size requested_swap_chain_size) {
  if (requested_swap_chain_size == swap_chain_size_ ||
      swap_chain_size_.IsEmpty() || requested_swap_chain_size.IsEmpty()) {
    frames_since_stable_swap_chain_resize_ = 0;
    last_requested_swap_chain_size_ = requested_swap_chain_size;
    return requested_swap_chain_size;
  }

  if (requested_swap_chain_size == last_requested_swap_chain_size_) {
    frames_since_stable_swap_chain_resize_++;
    // Resize after seeing the same requested size for 30 consecutive frames.
    if (frames_since_stable_swap_chain_resize_ >=
        kNumFramesBeforeSwapChainResize) {
      frames_since_stable_swap_chain_resize_ = 0;
      last_requested_swap_chain_size_ = requested_swap_chain_size;
      return requested_swap_chain_size;
    }
  } else {
    frames_since_stable_swap_chain_resize_ = 0;
    last_requested_swap_chain_size_ = requested_swap_chain_size;
  }

  // Keep the previous swap chain size
  return swap_chain_size_;
}

bool DCLayerTree::SwapChainPresenter::UploadVideoImages(
    gl::GLImageMemory* y_image_memory,
    gl::GLImageMemory* uv_image_memory) {
  gfx::Size texture_size = y_image_memory->GetSize();
  gfx::Size uv_image_size = uv_image_memory->GetSize();
  if (uv_image_size.height() != texture_size.height() / 2 ||
      uv_image_size.width() != texture_size.width() / 2 ||
      y_image_memory->format() != gfx::BufferFormat::R_8 ||
      uv_image_memory->format() != gfx::BufferFormat::RG_88) {
    DLOG(ERROR) << "Invalid NV12 GLImageMemory properties.";
    return false;
  }

  if (!staging_texture_ || (staging_texture_size_ != texture_size)) {
    staging_texture_.Reset();
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texture_size.width();
    desc.Height = texture_size.height();
    desc.Format = DXGI_FORMAT_NV12;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Usage = D3D11_USAGE_DYNAMIC;

    // This isn't actually bound to a decoder, but dynamic textures need
    // BindFlags to be nonzero and D3D11_BIND_DECODER also works when creating
    // a VideoProcessorInputView.
    desc.BindFlags = D3D11_BIND_DECODER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.SampleDesc.Count = 1;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = d3d11_device_->CreateTexture2D(
        &desc, nullptr, staging_texture_.GetAddressOf());
    if (FAILED(hr)) {
      DLOG(ERROR) << "Creating D3D11 video upload texture failed: " << std::hex
                  << hr;
      return false;
    }
    DCHECK(staging_texture_);
    staging_texture_size_ = texture_size;
  }
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  DCHECK(context);
  D3D11_MAPPED_SUBRESOURCE mapped_resource;
  HRESULT hr = context->Map(staging_texture_.Get(), 0, D3D11_MAP_WRITE_DISCARD,
                            0, &mapped_resource);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Mapping D3D11 video upload texture failed: " << std::hex
                << hr;
    return false;
  }

  size_t dest_stride = mapped_resource.RowPitch;
  for (int y = 0; y < texture_size.height(); y++) {
    const uint8_t* y_source =
        y_image_memory->memory() + y * y_image_memory->stride();
    uint8_t* dest =
        reinterpret_cast<uint8_t*>(mapped_resource.pData) + dest_stride * y;
    memcpy(dest, y_source, texture_size.width());
  }

  uint8_t* uv_dest_plane_start =
      reinterpret_cast<uint8_t*>(mapped_resource.pData) +
      dest_stride * texture_size.height();
  for (int y = 0; y < uv_image_size.height(); y++) {
    const uint8_t* uv_source =
        uv_image_memory->memory() + y * uv_image_memory->stride();
    uint8_t* dest = uv_dest_plane_start + dest_stride * y;
    memcpy(dest, uv_source, texture_size.width());
  }
  context->Unmap(staging_texture_.Get(), 0);
  return true;
}

gfx::Size DCLayerTree::SwapChainPresenter::CalculateSwapChainSize(
    const ui::DCRendererLayerParams& params) {
  // Swap chain size is the minimum of the on-screen size and the source
  // size so the video processor can do the minimal amount of work and
  // the overlay has to read the minimal amount of data.
  // DWM is also less likely to promote a surface to an overlay if it's
  // much larger than its area on-screen.

  // display_rect is the rect on screen
  gfx::RectF transformed_rect = gfx::RectF(params.rect);
  params.transform.TransformRect(&transformed_rect);
  gfx::Rect display_rect = gfx::ToEnclosingRect(transformed_rect);

  if (layer_tree_->workarounds().disable_larger_than_screen_overlays &&
      !g_overlay_monitor_size.IsEmpty()) {
    // Because of the rounding when converting between pixels and DIPs, a
    // fullscreen video can become slightly larger than the monitor - e.g. on
    // a 3000x2000 monitor with a scale factor of 1.75 a 1920x1079 video can
    // become 3002x1689.
    // On older Intel drivers, swapchains that are bigger than the monitor
    // won't be put into overlays, which will hurt power usage a lot. On those
    // systems, the scaling can be adjusted very slightly so that it's less
    // than the monitor size. This should be close to imperceptible.
    // TODO(jbauman): Remove when http://crbug.com/668278 is fixed.
    const int kOversizeMargin = 3;

    if ((display_rect.x() >= 0) &&
        (display_rect.width() > g_overlay_monitor_size.width()) &&
        (display_rect.width() <=
         g_overlay_monitor_size.width() + kOversizeMargin)) {
      display_rect.set_width(g_overlay_monitor_size.width());
    }

    if ((display_rect.y() >= 0) &&
        (display_rect.height() > g_overlay_monitor_size.height()) &&
        (display_rect.height() <=
         g_overlay_monitor_size.height() + kOversizeMargin)) {
      display_rect.set_height(g_overlay_monitor_size.height());
    }
  }

  // Downscaling doesn't work on Intel display HW, and so DWM will perform
  // an extra BLT to avoid HW downscaling. This prevents the use of hardware
  // overlays especially for protected video.
  gfx::Size swap_chain_size = display_rect.size();

  if (g_supports_scaled_overlays) {
    gfx::Size ceiled_input_size =
        gfx::ToCeiledSize(params.contents_rect.size());
    swap_chain_size.SetToMin(ceiled_input_size);
    // We don't want to recreate the swap chain too frequently, so override the
    // swap chain size with GetStableSwapChainSize().
    // TODO(magchen): This might cause overlay downscaling if the new size is
    // bigger than display_rect.
    swap_chain_size = GetStableSwapChainSize(swap_chain_size);
  }

  // YUV surfaces must have an even width.
  if (swap_chain_size.width() % 2 == 1)
    swap_chain_size.set_width(swap_chain_size.width() + 1);

  return swap_chain_size;
}

bool DCLayerTree::SwapChainPresenter::UpdateVisuals(
    const ui::DCRendererLayerParams& params,
    const gfx::Size& swap_chain_size) {
  bool needs_commit = false;

  if (!content_visual_) {
    DCHECK(!clip_visual_);
    dcomp_device_->CreateVisual(clip_visual_.GetAddressOf());
    DCHECK(clip_visual_);
    dcomp_device_->CreateVisual(content_visual_.GetAddressOf());
    DCHECK(content_visual_);
    clip_visual_->AddVisual(content_visual_.Get(), FALSE, nullptr);
    needs_commit = true;
  }

  // This is the scale from the swapchain size to the size of the contents
  // onscreen.
  float swap_chain_scale_x =
      params.rect.width() * 1.0f / swap_chain_size.width();
  float swap_chain_scale_y =
      params.rect.height() * 1.0f / swap_chain_size.height();
  gfx::Transform transform = params.transform;
  gfx::Transform scale_transform;
  scale_transform.Scale(swap_chain_scale_x, swap_chain_scale_y);
  transform.PreconcatTransform(scale_transform);
  transform.Transpose();

  // Offset is in layer space, and is applied before transform.
  // TODO(magchen): We should consider recalculating offset when it's non-zero.
  // Have not seen non-zero params.rect.x() and y() so far.
  gfx::Point offset(params.rect.x(), params.rect.y());

  if (visual_info_.transform != transform || visual_info_.offset != offset) {
    visual_info_.transform = transform;
    visual_info_.offset = offset;
    needs_commit = true;

    content_visual_->SetOffsetX(offset.x());
    content_visual_->SetOffsetY(offset.y());

    Microsoft::WRL::ComPtr<IDCompositionMatrixTransform> dcomp_transform;
    dcomp_device_->CreateMatrixTransform(dcomp_transform.GetAddressOf());
    DCHECK(dcomp_transform);
    D2D_MATRIX_3X2_F d2d_matrix = {
        {{transform.matrix().get(0, 0), transform.matrix().get(0, 1),
          transform.matrix().get(1, 0), transform.matrix().get(1, 1),
          transform.matrix().get(3, 0), transform.matrix().get(3, 1)}}};
    dcomp_transform->SetMatrix(d2d_matrix);
    content_visual_->SetTransform(dcomp_transform.Get());
  }

  if (visual_info_.is_clipped != params.is_clipped ||
      visual_info_.clip_rect != params.clip_rect) {
    visual_info_.is_clipped = params.is_clipped;
    visual_info_.clip_rect = params.clip_rect;
    needs_commit = true;
    // DirectComposition clips happen in the pre-transform visual
    // space, while cc/ clips happen post-transform. So the clip needs
    // to go on a separate parent visual that's untransformed.
    if (params.is_clipped) {
      Microsoft::WRL::ComPtr<IDCompositionRectangleClip> clip;
      dcomp_device_->CreateRectangleClip(clip.GetAddressOf());
      DCHECK(clip);
      const gfx::Rect& offset_clip = params.clip_rect;
      clip->SetLeft(offset_clip.x());
      clip->SetRight(offset_clip.right());
      clip->SetBottom(offset_clip.bottom());
      clip->SetTop(offset_clip.y());
      clip_visual_->SetClip(clip.Get());
    } else {
      clip_visual_->SetClip(nullptr);
    }
  }
  return needs_commit;
}

bool DCLayerTree::SwapChainPresenter::PresentToSwapChain(
    const ui::DCRendererLayerParams& params,
    bool* needs_commit) {
  *needs_commit = false;

  gl::GLImageDXGIBase* image_dxgi =
      gl::GLImageDXGIBase::FromGLImage(params.image[0].get());
  gl::GLImageMemory* y_image_memory = nullptr;
  gl::GLImageMemory* uv_image_memory = nullptr;
  if (params.image.size() >= 2) {
    y_image_memory = gl::GLImageMemory::FromGLImage(params.image[0].get());
    uv_image_memory = gl::GLImageMemory::FromGLImage(params.image[1].get());
  }

  if (!image_dxgi && (!y_image_memory || !uv_image_memory)) {
    DLOG(ERROR) << "Video GLImages are missing";
    last_gl_images_.clear();
    return false;
  }

  gfx::Size swap_chain_size = CalculateSwapChainSize(params);
  bool swap_chain_resized = swap_chain_size_ != swap_chain_size;
  // Do not create a swap chain if swap chain size will be empty.
  if (swap_chain_size.IsEmpty()) {
    if (swap_chain_resized) {
      swap_chain_size_ = swap_chain_size;
      swap_chain_handle_.Close();
      swap_chain_.Reset();
      out_view_.Reset();
      content_visual_->SetContent(nullptr);
      *needs_commit = true;
    }
    return true;
  }

  if (UpdateVisuals(params, swap_chain_size))
    *needs_commit = true;

  bool use_yuv_swap_chain = ShouldUseYUVSwapChain();
  bool toggle_yuv_swapchain = (use_yuv_swap_chain != is_yuv_swapchain_) &&
                              !failed_to_create_yuv_swapchain_;
  bool toggle_protected_video =
      is_protected_video_ != params.is_protected_video;

  bool first_present = false;
  if (!swap_chain_ || swap_chain_resized || toggle_yuv_swapchain ||
      toggle_protected_video) {
    if (!ReallocateSwapChain(swap_chain_size, use_yuv_swap_chain,
                             params.is_protected_video)) {
      return false;
    }
    first_present = true;
    content_visual_->SetContent(swap_chain_.Get());
    *needs_commit = true;
  } else if (last_gl_images_ == params.image) {
    // The swap chain is presenting the same images as last swap, which means
    // that the images were never returned to the video decoder and should
    // have the same contents as last time. It shouldn't need to be redrawn.
    return true;
  }

  last_gl_images_ = params.image;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture;
  UINT input_level;
  Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex;
  if (image_dxgi) {
    input_texture = image_dxgi->texture();
    input_level = (UINT)image_dxgi->level();
    if (!input_texture) {
      DLOG(ERROR) << "Video image has no texture";
      return false;
    }
    // Keyed mutex may not exist.
    keyed_mutex = image_dxgi->keyed_mutex();
    staging_texture_.Reset();
  } else {
    DCHECK(y_image_memory);
    DCHECK(uv_image_memory);
    if (!UploadVideoImages(y_image_memory, uv_image_memory))
      return false;
    DCHECK(staging_texture_);
    input_texture = staging_texture_;
    input_level = 0;
  }

  // TODO(sunnyps): Use correct color space for uploaded video frames.
  gfx::ColorSpace src_color_space = gfx::ColorSpace::CreateREC709();
  if (image_dxgi && image_dxgi->color_space().IsValid())
    src_color_space = image_dxgi->color_space();

  gfx::Size input_size = gfx::ToCeiledSize(params.contents_rect.size());

  if (!VideoProcessorBlt(input_texture, input_level, keyed_mutex, input_size,
                         src_color_space))
    return false;

  if (first_present) {
    HRESULT hr = swap_chain_->Present(0, 0);
    if (FAILED(hr)) {
      DLOG(ERROR) << "Present failed with error " << std::hex << hr;
      return false;
    }

    // DirectComposition can display black for a swap chain between the first
    // and second time it's presented to - maybe the first Present can get
    // lost somehow and it shows the wrong buffer. In that case copy the
    // buffers so both have the correct contents, which seems to help. The
    // first Present() after this needs to have SyncInterval > 0, or else the
    // workaround doesn't help.
    Microsoft::WRL::ComPtr<ID3D11Texture2D> dest_texture;
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(dest_texture.GetAddressOf()));
    DCHECK(dest_texture);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> src_texture;
    hr = swap_chain_->GetBuffer(1, IID_PPV_ARGS(src_texture.GetAddressOf()));
    DCHECK(src_texture);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    d3d11_device_->GetImmediateContext(context.GetAddressOf());
    DCHECK(context);
    context->CopyResource(dest_texture.Get(), src_texture.Get());

    // Additionally wait for the GPU to finish executing its commands, or
    // there still may be a black flicker when presenting expensive content
    // (e.g. 4k video).
    Microsoft::WRL::ComPtr<IDXGIDevice2> dxgi_device2;
    d3d11_device_.CopyTo(dxgi_device2.GetAddressOf());
    DCHECK(dxgi_device2);
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    hr = dxgi_device2->EnqueueSetEvent(event.handle());
    DCHECK(SUCCEEDED(hr));
    event.Wait();
  }

  HRESULT hr = swap_chain_->Present(1, 0);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Present failed with error " << std::hex << hr;
    return false;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "GPU.DirectComposition.SwapChainFormat2",
      is_yuv_swapchain_ ? g_overlay_format_used : OverlayFormat::kBGRA);

  frames_since_color_space_change_++;

  Microsoft::WRL::ComPtr<IDXGISwapChainMedia> swap_chain_media;
  if (SUCCEEDED(swap_chain_.CopyTo(swap_chain_media.GetAddressOf()))) {
    DCHECK(swap_chain_media);
    DXGI_FRAME_STATISTICS_MEDIA stats = {};
    if (SUCCEEDED(swap_chain_media->GetFrameStatisticsMedia(&stats))) {
      base::UmaHistogramSparse("GPU.DirectComposition.CompositionMode",
                               stats.CompositionMode);
      presentation_history_.AddSample(stats.CompositionMode);
    }
  }
  return true;
}

bool DCLayerTree::SwapChainPresenter::InitializeVideoProcessor(
    const gfx::Size& in_size,
    const gfx::Size& out_size) {
  if (video_processor_ && SizeContains(processor_input_size_, in_size) &&
      SizeContains(processor_output_size_, out_size))
    return true;
  processor_input_size_ = in_size;
  processor_output_size_ = out_size;

  if (!layer_tree_->InitializeVideoProcessor(in_size, out_size))
    return false;

  video_processor_enumerator_ = layer_tree_->video_processor_enumerator();
  video_processor_ = layer_tree_->video_processor();
  // out_view_ depends on video_processor_enumerator_, so ensure it's
  // recreated if the enumerator is.
  out_view_.Reset();
  return true;
}

bool DCLayerTree::SwapChainPresenter::VideoProcessorBlt(
    Microsoft::WRL::ComPtr<ID3D11Texture2D> input_texture,
    UINT input_level,
    Microsoft::WRL::ComPtr<IDXGIKeyedMutex> keyed_mutex,
    const gfx::Size& input_size,
    const gfx::ColorSpace& src_color_space) {
  if (!InitializeVideoProcessor(input_size, swap_chain_size_))
    return false;

  if (!out_view_) {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    swap_chain_->GetBuffer(0, IID_PPV_ARGS(texture.GetAddressOf()));
    D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC out_desc = {};
    out_desc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
    out_desc.Texture2D.MipSlice = 0;
    HRESULT hr = video_device_->CreateVideoProcessorOutputView(
        texture.Get(), video_processor_enumerator_.Get(), &out_desc,
        out_view_.GetAddressOf());
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateVideoProcessorOutputView failed with error "
                  << std::hex << hr;
      return false;
    }
  }

  gfx::ColorSpace output_color_space =
      is_yuv_swapchain_ ? src_color_space : gfx::ColorSpace::CreateSRGB();

  if (base::FeatureList::IsEnabled(kFallbackBT709VideoToBT601) &&
      (output_color_space == gfx::ColorSpace::CreateREC709())) {
    output_color_space = gfx::ColorSpace::CreateREC601();
  }

  Microsoft::WRL::ComPtr<IDXGISwapChain3> swap_chain3;
  Microsoft::WRL::ComPtr<ID3D11VideoContext1> context1;
  if (SUCCEEDED(swap_chain_.CopyTo(swap_chain3.GetAddressOf())) &&
      SUCCEEDED(video_context_.CopyTo(context1.GetAddressOf()))) {
    DCHECK(swap_chain3);
    DCHECK(context1);
    // Set input color space.
    context1->VideoProcessorSetStreamColorSpace1(
        video_processor_.Get(), 0,
        gfx::ColorSpaceWin::GetDXGIColorSpace(src_color_space));
    // Set output color space.
    DXGI_COLOR_SPACE_TYPE output_dxgi_color_space =
        gfx::ColorSpaceWin::GetDXGIColorSpace(
            output_color_space, is_yuv_swapchain_ /* force_yuv */);
    if (SUCCEEDED(swap_chain3->SetColorSpace1(output_dxgi_color_space))) {
      context1->VideoProcessorSetOutputColorSpace1(video_processor_.Get(),
                                                   output_dxgi_color_space);
    }
  } else {
    // This can't handle as many different types of color spaces, so use it
    // only if ID3D11VideoContext1 isn't available.
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE src_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(src_color_space);
    video_context_->VideoProcessorSetStreamColorSpace(video_processor_.Get(), 0,
                                                      &src_d3d11_color_space);
    D3D11_VIDEO_PROCESSOR_COLOR_SPACE output_d3d11_color_space =
        gfx::ColorSpaceWin::GetD3D11ColorSpace(output_color_space);
    video_context_->VideoProcessorSetOutputColorSpace(
        video_processor_.Get(), &output_d3d11_color_space);
  }

  {
    base::Optional<ScopedReleaseKeyedMutex> release_keyed_mutex;
    if (keyed_mutex) {
      // The producer may still be using this texture for a short period of
      // time, so wait long enough to hopefully avoid glitches. For example,
      // all levels of the texture share the same keyed mutex, so if the
      // hardware decoder acquired the mutex to decode into a different array
      // level then it still may block here temporarily.
      const int kMaxSyncTimeMs = 1000;
      HRESULT hr = keyed_mutex->AcquireSync(0, kMaxSyncTimeMs);
      if (FAILED(hr)) {
        DLOG(ERROR) << "Error acquiring keyed mutex: " << std::hex << hr;
        return false;
      }
      release_keyed_mutex.emplace(keyed_mutex, 0);
    }

    D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC in_desc = {};
    in_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
    in_desc.Texture2D.ArraySlice = input_level;
    Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> in_view;
    HRESULT hr = video_device_->CreateVideoProcessorInputView(
        input_texture.Get(), video_processor_enumerator_.Get(), &in_desc,
        in_view.GetAddressOf());
    if (FAILED(hr)) {
      DLOG(ERROR) << "CreateVideoProcessorInputView failed with error "
                  << std::hex << hr;
      return false;
    }

    D3D11_VIDEO_PROCESSOR_STREAM stream = {};
    stream.Enable = true;
    stream.OutputIndex = 0;
    stream.InputFrameOrField = 0;
    stream.PastFrames = 0;
    stream.FutureFrames = 0;
    stream.pInputSurface = in_view.Get();
    RECT dest_rect = gfx::Rect(swap_chain_size_).ToRECT();
    video_context_->VideoProcessorSetOutputTargetRect(video_processor_.Get(),
                                                      TRUE, &dest_rect);
    video_context_->VideoProcessorSetStreamDestRect(video_processor_.Get(), 0,
                                                    TRUE, &dest_rect);
    RECT source_rect = gfx::Rect(input_size).ToRECT();
    video_context_->VideoProcessorSetStreamSourceRect(video_processor_.Get(), 0,
                                                      TRUE, &source_rect);

    hr = video_context_->VideoProcessorBlt(video_processor_.Get(),
                                           out_view_.Get(), 0, 1, &stream);
    if (FAILED(hr)) {
      DLOG(ERROR) << "VideoProcessorBlt failed with error " << std::hex << hr;
      return false;
    }
  }

  return true;
}

bool DCLayerTree::SwapChainPresenter::ReallocateSwapChain(
    const gfx::Size& swap_chain_size,
    bool use_yuv_swap_chain,
    bool protected_video) {
  TRACE_EVENT0("gpu", "DCLayerTree::SwapChainPresenter::ReallocateSwapChain");
  swap_chain_size_ = swap_chain_size;

  out_view_.Reset();
  swap_chain_.Reset();
  swap_chain_handle_.Close();

  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device_.CopyTo(dxgi_device.GetAddressOf());
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactoryMedia> media_factory;
  dxgi_adapter->GetParent(IID_PPV_ARGS(media_factory.GetAddressOf()));
  DCHECK(media_factory);

  DXGI_SWAP_CHAIN_DESC1 desc = {};
  DCHECK(!swap_chain_size_.IsEmpty());
  desc.Width = swap_chain_size_.width();
  desc.Height = swap_chain_size_.height();
  desc.Format = g_overlay_dxgi_format_used;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferCount = 2;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
  desc.Flags =
      DXGI_SWAP_CHAIN_FLAG_YUV_VIDEO | DXGI_SWAP_CHAIN_FLAG_FULLSCREEN_VIDEO;
  if (protected_video) {
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
    desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
  }

  HANDLE handle;
  if (!CreateSurfaceHandleHelper(&handle))
    return false;

  swap_chain_handle_.Set(handle);

  if (is_yuv_swapchain_ != use_yuv_swap_chain) {
    UMA_HISTOGRAM_COUNTS_1000(
        "GPU.DirectComposition.FramesSinceColorSpaceChange",
        frames_since_color_space_change_);
  }

  frames_since_color_space_change_ = 0;

  const std::string kSwapChainCreationResultUmaPrefix =
      "GPU.DirectComposition.SwapChainCreationResult.";
  is_yuv_swapchain_ = false;
  // The composition surface handle isn't actually used, but
  // CreateSwapChainForComposition can't create YUV swapchains.
  if (use_yuv_swap_chain) {
    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        swap_chain_.GetAddressOf());
    is_yuv_swapchain_ = SUCCEEDED(hr);
    failed_to_create_yuv_swapchain_ = !is_yuv_swapchain_;
    base::UmaHistogramBoolean(kSwapChainCreationResultUmaPrefix +
                                  OverlayFormatToString(g_overlay_format_used),
                              SUCCEEDED(hr));
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create "
                  << OverlayFormatToString(g_overlay_format_used)
                  << " swap chain with error " << std::hex << hr
                  << ". Falling back to BGRA";
    }
  }

  if (!is_yuv_swapchain_) {
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Flags = 0;
    if (protected_video) {
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_HW_PROTECTED;
      desc.Flags |= DXGI_SWAP_CHAIN_FLAG_DISPLAY_ONLY;
    }
    HRESULT hr = media_factory->CreateSwapChainForCompositionSurfaceHandle(
        d3d11_device_.Get(), swap_chain_handle_.Get(), &desc, nullptr,
        swap_chain_.GetAddressOf());
    base::UmaHistogramBoolean(kSwapChainCreationResultUmaPrefix +
                                  OverlayFormatToString(OverlayFormat::kBGRA),
                              SUCCEEDED(hr));
    if (FAILED(hr)) {
      DLOG(ERROR) << "Failed to create BGRA swap chain with error " << std::hex
                  << hr;
      return false;
    }
  }
  is_protected_video_ = protected_video;
  return true;
}

bool DCLayerTree::UpdateBackbufferVisual(BackbufferInfo info) {
  bool needs_commit = false;

  if (!backbuffer_visual_) {
    dcomp_device_->CreateVisual(backbuffer_visual_.GetAddressOf());
    needs_commit = true;
  }

  if (backbuffer_info_ != info) {
    backbuffer_info_ = std::move(info);
    backbuffer_visual_->SetContent(
        backbuffer_info_.dcomp_surface
            ? static_cast<IUnknown*>(backbuffer_info_.dcomp_surface.Get())
            : static_cast<IUnknown*>(backbuffer_info_.swap_chain.Get()));
    needs_commit = true;
  }

  return needs_commit;
}

bool DCLayerTree::CommitAndClearPendingOverlays(
    BackbufferInfo backbuffer_info) {
  bool needs_commit = false;

  // Check if backbuffer visual needs a commit first.
  if (UpdateBackbufferVisual(std::move(backbuffer_info)))
    needs_commit = true;

  // Sort layers by z-order.
  std::sort(pending_overlays_.begin(), pending_overlays_.end(),
            [](const auto& a, const auto& b) -> bool {
              return a->z_order < b->z_order;
            });

  // If we need to grow or shrink swap chain presenters, we'll need to add or
  // remove visuals.
  if (video_swap_chains_.size() != pending_overlays_.size()) {
    // Grow or shrink list of swap chain presenters to match pending overlays.
    std::vector<std::unique_ptr<SwapChainPresenter>> new_video_swap_chains;
    for (size_t i = 0; i < pending_overlays_.size(); ++i) {
      // TODO(sunnyps): Try to find a matching swap chain based on size, type of
      // swap chain, gl image, etc.
      if (i < video_swap_chains_.size()) {
        new_video_swap_chains.emplace_back(std::move(video_swap_chains_[i]));
      } else {
        new_video_swap_chains.emplace_back(std::make_unique<SwapChainPresenter>(
            this, d3d11_device_, dcomp_device_, video_device_, video_context_));
      }
    }
    video_swap_chains_.swap(new_video_swap_chains);
    needs_commit = true;
  }

  // Present to each swap chain.
  for (size_t i = 0; i < pending_overlays_.size(); ++i) {
    auto& video_swap_chain = video_swap_chains_[i];
    bool video_needs_commit = false;
    if (!video_swap_chain->PresentToSwapChain(*pending_overlays_[i],
                                              &video_needs_commit)) {
      return false;
    }
    needs_commit = needs_commit || video_needs_commit;
  }

  // Rebuild visual tree and commit if any visual changed.
  if (needs_commit) {
    root_visual_->RemoveAllVisuals();

    // Add layers with negative z-order first.
    size_t i = 0;
    for (; i < pending_overlays_.size() && pending_overlays_[i]->z_order < 0;
         ++i) {
      IDCompositionVisual2* visual = video_swap_chains_[i]->visual().Get();
      // We call AddVisual with insertAbove FALSE and referenceVisual nullptr
      // which is equivalent to saying that the visual should be below no other
      // visual, or in other words it should be above all other visuals.
      root_visual_->AddVisual(visual, FALSE, nullptr);
    }

    // Add backbuffer visual at z-order 0.
    root_visual_->AddVisual(backbuffer_visual_.Get(), FALSE, nullptr);

    // Add visuals with positive z-order.
    for (; i < pending_overlays_.size(); ++i) {
      // There shouldn't be a layer with z-order 0.  Otherwise, we can't tell
      // its order with respect to backbuffer.
      DCHECK_GT(pending_overlays_[i]->z_order, 0);
      IDCompositionVisual2* visual = video_swap_chains_[i]->visual().Get();
      root_visual_->AddVisual(visual, FALSE, nullptr);
    }

    HRESULT hr = dcomp_device_->Commit();
    if (FAILED(hr)) {
      DLOG(ERROR) << "Commit failed with error " << std::hex << hr;
      return false;
    }
  }

  pending_overlays_.clear();
  return true;
}

bool DCLayerTree::ScheduleDCLayer(const ui::DCRendererLayerParams& params) {
  pending_overlays_.push_back(
      std::make_unique<ui::DCRendererLayerParams>(params));
  return true;
}

DirectCompositionSurfaceWin::DirectCompositionSurfaceWin(
    std::unique_ptr<gfx::VSyncProvider> vsync_provider,
    base::WeakPtr<ImageTransportSurfaceDelegate> delegate,
    HWND parent_window)
    : gl::GLSurfaceEGL(),
      child_window_(delegate, parent_window),
      vsync_provider_(std::move(vsync_provider)),
      layer_tree_(std::make_unique<DCLayerTree>(
          delegate->GetFeatureInfo()->workarounds())) {}

DirectCompositionSurfaceWin::~DirectCompositionSurfaceWin() {
  Destroy();
}

// static
bool DirectCompositionSurfaceWin::AreOverlaysSupported() {
  // Check for DirectComposition support first to prevent likely crashes.
  if (!IsDirectCompositionSupported())
    return false;

  // Always initialize and record overlay support information irrespective of
  // command line flags.
  InitializeHardwareOverlaySupport();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableDirectCompositionLayers))
    return false;
  if (command_line->HasSwitch(switches::kEnableDirectCompositionLayers))
    return true;

  return g_supports_overlays;
}

// static
OverlayCapabilities DirectCompositionSurfaceWin::GetOverlayCapabilities() {
  InitializeHardwareOverlaySupport();

  OverlayCapabilities capabilities;
  for (const auto& info : g_overlay_support_info) {
    if (info.flags) {
      OverlayCapability cap;
      cap.format = info.overlay_format;
      cap.is_scaling_supported =
          !!(info.flags & DXGI_OVERLAY_SUPPORT_FLAG_SCALING);
      capabilities.push_back(cap);
    }
  }

  return capabilities;
}

// static
void DirectCompositionSurfaceWin::SetScaledOverlaysSupportedForTesting(
    bool value) {
  g_supports_scaled_overlays = value;
}

// static
int DirectCompositionSurfaceWin::GetNumFramesBeforeSwapChainResizeForTesting() {
  return kNumFramesBeforeSwapChainResize;
}

// static
bool DirectCompositionSurfaceWin::IsHDRSupported() {
  // HDR support was introduced in Windows 10 Creators Update.
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS2)
    return false;

  HRESULT hr = S_OK;
  Microsoft::WRL::ComPtr<IDXGIFactory> factory;
  hr = CreateDXGIFactory(__uuidof(IDXGIFactory),
                         reinterpret_cast<void**>(factory.GetAddressOf()));
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create DXGI factory.";
    return false;
  }

  bool hdr_monitor_found = false;
  for (UINT adapter_index = 0;; ++adapter_index) {
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    hr = factory->EnumAdapters(adapter_index, adapter.GetAddressOf());
    if (hr == DXGI_ERROR_NOT_FOUND)
      break;
    if (FAILED(hr)) {
      DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
      break;
    }

    for (UINT output_index = 0;; ++output_index) {
      Microsoft::WRL::ComPtr<IDXGIOutput> output;
      hr = adapter->EnumOutputs(output_index, output.GetAddressOf());
      if (hr == DXGI_ERROR_NOT_FOUND)
        break;
      if (FAILED(hr)) {
        DLOG(ERROR) << "Unexpected error creating DXGI adapter.";
        break;
      }

      Microsoft::WRL::ComPtr<IDXGIOutput6> output6;
      hr = output->QueryInterface(
          __uuidof(IDXGIOutput6),
          reinterpret_cast<void**>(output6.GetAddressOf()));
      if (FAILED(hr)) {
        DLOG(WARNING) << "IDXGIOutput6 is required for HDR detection.";
        continue;
      }

      DXGI_OUTPUT_DESC1 desc;
      if (FAILED(output6->GetDesc1(&desc))) {
        DLOG(ERROR) << "Unexpected error getting output descriptor.";
        continue;
      }

      base::UmaHistogramSparse("GPU.Output.ColorSpace", desc.ColorSpace);
      base::UmaHistogramSparse("GPU.Output.MaxLuminance", desc.MaxLuminance);

      if (desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020) {
        hdr_monitor_found = true;
      }
    }
  }

  UMA_HISTOGRAM_BOOLEAN("GPU.Output.HDR", hdr_monitor_found);
  return hdr_monitor_found;
}

// static
bool DirectCompositionSurfaceWin::IsSwapChainTearingSupported() {
  static bool initialized = false;
  static bool supported = false;

  if (initialized)
    return supported;

  initialized = true;

  // Swap chain tearing is used only if vsync is disabled explicitly.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableGpuVsync))
    return false;

  // Swap chain tearing is supported only on Windows 10 Anniversary Edition
  // (Redstone 1) and above.
  if (base::win::GetVersion() < base::win::VERSION_WIN10_RS1)
    return false;

  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device) {
    DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                   "D3D11 device from ANGLE";
    return false;
  }
  Microsoft::WRL::ComPtr<IDXGIDevice> dxgi_device;
  d3d11_device.CopyTo(dxgi_device.GetAddressOf());
  DCHECK(dxgi_device);
  Microsoft::WRL::ComPtr<IDXGIAdapter> dxgi_adapter;
  dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf());
  DCHECK(dxgi_adapter);
  Microsoft::WRL::ComPtr<IDXGIFactory5> dxgi_factory;
  if (FAILED(
          dxgi_adapter->GetParent(IID_PPV_ARGS(dxgi_factory.GetAddressOf())))) {
    DLOG(ERROR) << "Not using swap chain tearing because failed to retrieve "
                   "IDXGIFactory5 interface";
    return false;
  }

  // BOOL instead of bool because we want a well defined sized type.
  BOOL present_allow_tearing = FALSE;
  DCHECK(dxgi_factory);
  if (FAILED(dxgi_factory->CheckFeatureSupport(
          DXGI_FEATURE_PRESENT_ALLOW_TEARING, &present_allow_tearing,
          sizeof(present_allow_tearing)))) {
    DLOG(ERROR)
        << "Not using swap chain tearing because CheckFeatureSupport failed";
    return false;
  }
  supported = !!present_allow_tearing;
  return supported;
}

bool DirectCompositionSurfaceWin::InitializeNativeWindow() {
  if (window_)
    return true;

  bool result = child_window_.Initialize();
  window_ = child_window_.window();
  return result;
}

bool DirectCompositionSurfaceWin::Initialize(gl::GLSurfaceFormat format) {
  d3d11_device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device_) {
    DLOG(ERROR) << "Failed to retrieve D3D11 device from ANGLE";
    return false;
  }

  dcomp_device_ = gl::QueryDirectCompositionDevice(d3d11_device_);
  if (!dcomp_device_) {
    DLOG(ERROR)
        << "Failed to retrieve direct compostion device from D3D11 device";
    return false;
  }

  EGLDisplay display = GetDisplay();
  if (!window_) {
    if (!InitializeNativeWindow()) {
      DLOG(ERROR) << "Failed to initialize native window";
      return false;
    }
  }

  if (!layer_tree_->Initialize(window_, d3d11_device_, dcomp_device_))
    return false;

  EGLint pbuffer_attribs[] = {
      EGL_WIDTH,
      1,
      EGL_HEIGHT,
      1,
      EGL_FLEXIBLE_SURFACE_COMPATIBILITY_SUPPORTED_ANGLE,
      EGL_TRUE,
      EGL_NONE,
  };

  default_surface_ =
      eglCreatePbufferSurface(display, GetConfig(), pbuffer_attribs);
  if (!default_surface_) {
    DLOG(ERROR) << "eglCreatePbufferSurface failed with error "
                << ui::GetLastEGLErrorString();
    return false;
  }

  if (!RecreateRootSurface())
    return false;

  presentation_helper_ =
      std::make_unique<gl::GLSurfacePresentationHelper>(vsync_provider_.get());
  return true;
}

void DirectCompositionSurfaceWin::Destroy() {
  presentation_helper_ = nullptr;
  if (default_surface_) {
    if (!eglDestroySurface(GetDisplay(), default_surface_)) {
      DLOG(ERROR) << "eglDestroySurface failed with error "
                  << ui::GetLastEGLErrorString();
    }
    default_surface_ = nullptr;
  }
  if (root_surface_) {
    root_surface_->Destroy();
    root_surface_ = nullptr;
  }
}

gfx::Size DirectCompositionSurfaceWin::GetSize() {
  return size_;
}

bool DirectCompositionSurfaceWin::IsOffscreen() {
  return false;
}

void* DirectCompositionSurfaceWin::GetHandle() {
  return root_surface_ ? root_surface_->GetHandle() : default_surface_;
}

bool DirectCompositionSurfaceWin::Resize(const gfx::Size& size,
                                         float scale_factor,
                                         ColorSpace color_space,
                                         bool has_alpha) {
  bool is_hdr = color_space == ColorSpace::SCRGB_LINEAR;
  if (size == GetSize() && has_alpha == has_alpha_ && is_hdr == is_hdr_)
    return true;

  // Force a resize and redraw (but not a move, activate, etc.).
  if (!SetWindowPos(window_, nullptr, 0, 0, size.width(), size.height(),
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOCOPYBITS |
                        SWP_NOOWNERZORDER | SWP_NOZORDER)) {
    return false;
  }
  size_ = size;
  is_hdr_ = is_hdr;
  has_alpha_ = has_alpha;
  return RecreateRootSurface();
}

gfx::SwapResult DirectCompositionSurfaceWin::SwapBuffers(
    const PresentationCallback& callback) {
  gl::GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), callback);

  bool succeeded = true;
  DCHECK(root_surface_);
  if (root_surface_->SwapBuffers(PresentationCallback()) ==
      gfx::SwapResult::SWAP_FAILED)
    succeeded = false;

  DCLayerTree::BackbufferInfo backbuffer_info = {
      root_surface_->swap_chain().Get(), root_surface_->dcomp_surface().Get(),
      root_surface_->dcomp_surface_serial(),
  };
  if (!layer_tree_->CommitAndClearPendingOverlays(std::move(backbuffer_info)))
    succeeded = false;

  UMA_HISTOGRAM_BOOLEAN("GPU.DirectComposition.SwapBuffersResult", succeeded);

  auto swap_result =
      succeeded ? gfx::SwapResult::SWAP_ACK : gfx::SwapResult::SWAP_FAILED;
  scoped_swap_buffers.set_result(swap_result);
  return swap_result;
}

gfx::SwapResult DirectCompositionSurfaceWin::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    const PresentationCallback& callback) {
  // The arguments are ignored because SetDrawRectangle specified the area to
  // be swapped.
  return SwapBuffers(callback);
}

gfx::VSyncProvider* DirectCompositionSurfaceWin::GetVSyncProvider() {
  return vsync_provider_.get();
}

void DirectCompositionSurfaceWin::SetVSyncEnabled(bool enabled) {
  vsync_enabled_ = enabled;
  if (root_surface_)
    root_surface_->SetVSyncEnabled(enabled);
}

bool DirectCompositionSurfaceWin::ScheduleDCLayer(
    const ui::DCRendererLayerParams& params) {
  return layer_tree_->ScheduleDCLayer(params);
}

bool DirectCompositionSurfaceWin::SetEnableDCLayers(bool enable) {
  if (enable_dc_layers_ == enable)
    return true;
  enable_dc_layers_ = enable;
  return RecreateRootSurface();
}

bool DirectCompositionSurfaceWin::FlipsVertically() const {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPresentationCallback() {
  return true;
}

bool DirectCompositionSurfaceWin::SupportsPostSubBuffer() {
  return true;
}

bool DirectCompositionSurfaceWin::OnMakeCurrent(gl::GLContext* context) {
  if (presentation_helper_)
    presentation_helper_->OnMakeCurrent(context, this);
  if (root_surface_)
    return root_surface_->OnMakeCurrent(context);
  return true;
}

bool DirectCompositionSurfaceWin::SupportsDCLayers() const {
  return true;
}

bool DirectCompositionSurfaceWin::UseOverlaysForVideo() const {
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SupportsProtectedVideo() const {
  // TODO(magchen): Check the gpu driver date (or a function) which we know this
  // new support is enabled.
  return AreOverlaysSupported();
}

bool DirectCompositionSurfaceWin::SetDrawRectangle(const gfx::Rect& rectangle) {
  if (root_surface_)
    return root_surface_->SetDrawRectangle(rectangle);
  return false;
}

gfx::Vector2d DirectCompositionSurfaceWin::GetDrawOffset() const {
  if (root_surface_)
    return root_surface_->GetDrawOffset();
  return gfx::Vector2d();
}

bool DirectCompositionSurfaceWin::RecreateRootSurface() {
  root_surface_ = new DirectCompositionChildSurfaceWin(
      size_, is_hdr_, has_alpha_, enable_dc_layers_,
      IsSwapChainTearingSupported());
  root_surface_->SetVSyncEnabled(vsync_enabled_);
  return root_surface_->Initialize();
}

scoped_refptr<base::TaskRunner>
DirectCompositionSurfaceWin::GetWindowTaskRunnerForTesting() {
  return child_window_.GetTaskRunnerForTesting();
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetLayerSwapChainForTesting(size_t index) const {
  return layer_tree_->GetLayerSwapChainForTesting(index);
}

Microsoft::WRL::ComPtr<IDXGISwapChain1>
DirectCompositionSurfaceWin::GetBackbufferSwapChainForTesting() const {
  if (root_surface_)
    return root_surface_->swap_chain();
  return nullptr;
}

}  // namespace gpu
