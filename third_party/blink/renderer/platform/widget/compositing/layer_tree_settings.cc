// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/compositing/layer_tree_settings.h"

#include <algorithm>
#include <tuple>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/switches.h"
#include "cc/tiles/image_decode_cache_utils.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "media/base/media_switches.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/ui_base_switches.h"
#include "ui/native_theme/native_theme_features.h"
#include "ui/native_theme/overlay_scrollbar_constants_aura.h"

namespace blink {

namespace {

BASE_FEATURE(kUnpremultiplyAndDitherLowBitDepthTiles,
             "UnpremultiplyAndDitherLowBitDepthTiles",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, scrollbar fade animations' delay and duration are scaled
// according to `kFadeDelayScalingFactor` and `kFadeDurationScalingFactor`
// below, respectively. For more context, please see https://crbug.com/1245964.
BASE_FEATURE(kScaleScrollbarAnimationTiming,
             "ScaleScrollbarAnimationTiming",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureParam<double> kFadeDelayScalingFactor{
    &kScaleScrollbarAnimationTiming, "fade_delay_scaling_factor",
    /*default_value=*/1.0};

constexpr base::FeatureParam<double> kFadeDurationScalingFactor{
    &kScaleScrollbarAnimationTiming, "fade_duration_scaling_factor",
    /*default_value=*/1.0};

void InitializeScrollbarFadeAndDelay(cc::LayerTreeSettings& settings) {
  // Default settings that may be overridden below for specific platforms.
  settings.scrollbar_fade_delay = base::Milliseconds(300);
  settings.scrollbar_fade_duration = base::Milliseconds(300);

#if !BUILDFLAG(IS_ANDROID)
  if (ui::IsOverlayScrollbarEnabled()) {
    settings.idle_thickness_scale = ui::kOverlayScrollbarIdleThicknessScale;
    if (ui::IsFluentOverlayScrollbarEnabled()) {
      settings.scrollbar_fade_delay = ui::kFluentOverlayScrollbarFadeDelay;
      settings.scrollbar_fade_duration =
          ui::kFluentOverlayScrollbarFadeDuration;
    } else {
      settings.scrollbar_fade_delay = ui::kOverlayScrollbarFadeDelay;
      settings.scrollbar_fade_duration = ui::kOverlayScrollbarFadeDuration;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (base::FeatureList::IsEnabled(kScaleScrollbarAnimationTiming)) {
    settings.scrollbar_fade_delay *= kFadeDelayScalingFactor.Get();
    settings.scrollbar_fade_duration *= kFadeDurationScalingFactor.Get();
  }
}

#if BUILDFLAG(IS_ANDROID)
// With 32 bit pixels, this would mean less than 400kb per buffer. Much less
// than required for, say, nHD.
static const int kSmallScreenPixelThreshold = 1e5;
bool IsSmallScreen(const gfx::Size& size) {
  int area = 0;
  if (!size.GetCheckedArea().AssignIfValid(&area))
    return false;
  return area < kSmallScreenPixelThreshold;
}
#endif

std::pair<int, int> GetTilingInterestAreaSizes() {
  int interest_area_size_in_pixels;

  if (base::FeatureList::IsEnabled(::features::kSmallerInterestArea) &&
      ::features::kInterestAreaSizeInPixels.Get() ==
          ::features::kInterestAreaSizeInPixels.default_value) {
    interest_area_size_in_pixels =
        ::features::kDefaultInterestAreaSizeInPixelsWhenEnabled;
  } else {
    interest_area_size_in_pixels = ::features::kInterestAreaSizeInPixels.Get();
  }

  if (interest_area_size_in_pixels ==
      ::features::kInterestAreaSizeInPixels.default_value) {
    return {
        ::features::kDefaultInterestAreaSizeInPixels,
        cc::LayerTreeSettings::kDefaultSkewportExtrapolationLimitInScrenPixels};
  }
  // Keep the same ratio we have by default.
  static_assert(
      cc::LayerTreeSettings::kDefaultSkewportExtrapolationLimitInScrenPixels ==
      2 * ::features::kDefaultInterestAreaSizeInPixels / 3);
  return {interest_area_size_in_pixels, (2 * interest_area_size_in_pixels) / 3};
}

#if !BUILDFLAG(IS_ANDROID)
// Adjusting tile memory size in case a lot more websites need more tile
// memory than the current calculation.
BASE_FEATURE(kAdjustTileGpuMemorySize,
             "AdjustTileGpuMemorySize",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr size_t kLargeResolutionMemoryMB = 1152;
constexpr size_t kDefaultMemoryMB = 512;

constexpr base::FeatureParam<int> kNewLargeResolutionMemoryMB{
    &kAdjustTileGpuMemorySize, "new_large_resolution_memory_mb",
    /*default_value=*/kLargeResolutionMemoryMB};

constexpr base::FeatureParam<int> kNewDefaultMemoryMB{
    &kAdjustTileGpuMemorySize, "new_default_memory_mb",
    /*default_value=*/kDefaultMemoryMB};

size_t GetLargeResolutionMemoryMB() {
  if (base::FeatureList::IsEnabled(kAdjustTileGpuMemorySize)) {
    return kNewLargeResolutionMemoryMB.Get();
  } else {
    return kLargeResolutionMemoryMB;
  }
}

size_t GetDefaultMemoryMB() {
  if (base::FeatureList::IsEnabled(kAdjustTileGpuMemorySize)) {
    return kNewDefaultMemoryMB.Get();
  } else {
    return kDefaultMemoryMB;
  }
}
#endif

}  // namespace

// static
cc::ManagedMemoryPolicy GetGpuMemoryPolicy(
    const cc::ManagedMemoryPolicy& default_policy,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor) {
  cc::ManagedMemoryPolicy actual = default_policy;
  actual.bytes_limit_when_visible = 0;

  // If the value was overridden on the command line, use the specified value.
  static bool client_hard_limit_bytes_overridden =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kForceGpuMemAvailableMb);
  if (client_hard_limit_bytes_overridden) {
    if (base::StringToSizeT(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                ::switches::kForceGpuMemAvailableMb),
            &actual.bytes_limit_when_visible))
      actual.bytes_limit_when_visible *= 1024 * 1024;
    return actual;
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::IsLowEndDevice() ||
      base::SysInfo::AmountOfPhysicalMemoryMB() < 2000) {
    actual.bytes_limit_when_visible = 96 * 1024 * 1024;
  } else {
    actual.bytes_limit_when_visible = 256 * 1024 * 1024;
  }
#else
  // This calculation will increase the tile memory size. It should apply to
  // the other plateforms if no regression on Mac.
  //
  // For large monitors with high resolution, increase the tile memory to
  // avoid frequent out of memory problems. With Mac M1 on
  // https://www.334-28th.com/, it seems 512 MB works fine on 1920x1080 * 2
  // (scale) and 1152 MB on 2056x1329 * 2 (scale). Use this ratio for the
  // formula to increase |bytes_limit_when_visible| proportionally.
  // For mobile platforms with small display (roughly less than 3k x 1.6k),
  // mb_limit will still be 512 MB.
  constexpr size_t kLargeResolution = 2056 * 1329 * 2 * 2;
  size_t display_size =
      std::round(initial_screen_size.width() * initial_device_scale_factor *
                 initial_screen_size.height() * initial_device_scale_factor);

  size_t large_resolution_memory_mb = GetLargeResolutionMemoryMB();
  size_t mb_limit_when_visible =
      large_resolution_memory_mb * (display_size * 1.0 / kLargeResolution);

  // Cap the memory size to one fourth of the total system memory so it won't
  // consume too much of the system memory. Still keep the minimum to the
  // default of 512MB.
  size_t default_memory_mb = GetDefaultMemoryMB();
  size_t memory_cap_mb = base::SysInfo::AmountOfPhysicalMemoryMB() / 4;
  if (mb_limit_when_visible > memory_cap_mb) {
    mb_limit_when_visible = memory_cap_mb;
  } else if (mb_limit_when_visible < default_memory_mb) {
    mb_limit_when_visible = default_memory_mb;
  }

  actual.bytes_limit_when_visible = mb_limit_when_visible * 1024 * 1024;
#endif
  actual.priority_cutoff_when_visible =
      gpu::MemoryAllocation::CUTOFF_ALLOW_NICE_TO_HAVE;

  return actual;
}

// static
cc::LayerTreeSettings GenerateLayerTreeSettings(
    bool is_threaded,
    bool is_for_embedded_frame,
    bool is_for_scalable_page,
    const gfx::Size& initial_screen_size,
    float initial_device_scale_factor) {
  const base::CommandLine& cmd = *base::CommandLine::ForCurrentProcess();
  cc::LayerTreeSettings settings;

  settings.enable_synchronized_scrolling =
      base::FeatureList::IsEnabled(::features::kSynchronizedScrolling);
  Platform* platform = Platform::Current();
  settings.percent_based_scrolling =
      ::features::IsPercentBasedScrollingEnabled();

  settings.commit_to_active_tree = !is_threaded;
  settings.is_for_embedded_frame = is_for_embedded_frame;
  settings.is_for_scalable_page = is_for_scalable_page;

  settings.main_frame_before_activation_enabled =
      cmd.HasSwitch(cc::switches::kEnableMainFrameBeforeActivation);

  // Checkerimaging is not supported for synchronous single-threaded mode, which
  // is what the renderer uses if its not threaded.
  settings.enable_checker_imaging =
      !cmd.HasSwitch(cc::switches::kDisableCheckerImaging) && is_threaded;

#if BUILDFLAG(IS_ANDROID)
  // WebView should always raster in the default color space.
  // Synchronous compositing indicates WebView.
  if (!platform->IsSynchronousCompositingEnabledForAndroidWebView())
    settings.prefer_raster_in_srgb = ::features::IsDynamicColorGamutEnabled();

  // We can use a more aggressive limit on Android since decodes tend to take
  // longer on these devices.
  settings.min_image_bytes_to_checker = 512 * 1024;  // 512kB

  // Re-rasterization of checker-imaged content with software raster can be too
  // costly on Android.
  settings.only_checker_images_with_gpu_raster = true;
#endif

  auto switch_value_as_int = [](const base::CommandLine& command_line,
                                const std::string& switch_string, int min_value,
                                int max_value, int* result) {
    std::string string_value = command_line.GetSwitchValueASCII(switch_string);
    int int_value;
    if (base::StringToInt(string_value, &int_value) && int_value >= min_value &&
        int_value <= max_value) {
      *result = int_value;
      return true;
    } else {
      DLOG(WARNING) << "Failed to parse switch " << switch_string << ": "
                    << string_value;
      return false;
    }
  };

  int default_tile_size = 256;
#if BUILDFLAG(IS_ANDROID)
  const gfx::Size screen_size =
      gfx::ScaleToFlooredSize(initial_screen_size, initial_device_scale_factor);
  int display_width = screen_size.width();
  int display_height = screen_size.height();
  int numTiles = (display_width * display_height) / (256 * 256);
  if (numTiles > 16)
    default_tile_size = 384;
  if (numTiles >= 40)
    default_tile_size = 512;

  // Adjust for some resolutions that barely straddle an extra
  // tile when in portrait mode. This helps worst case scroll/raster
  // by not needing a full extra tile for each row.
  constexpr int tolerance = 10;  // To avoid rounding errors.
  int portrait_width = std::min(display_width, display_height);
  if (default_tile_size == 256 && std::abs(portrait_width - 768) < tolerance)
    default_tile_size += 32;
  if (default_tile_size == 384 && std::abs(portrait_width - 1200) < tolerance)
    default_tile_size += 32;
#elif BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  // Use 512 for high DPI (dsf=2.0f) devices.
  if (initial_device_scale_factor >= 2.0f)
    default_tile_size = 512;
#endif

  // TODO(danakj): This should not be a setting O_O; it should change when the
  // device scale factor on LayerTreeHost changes.
  settings.default_tile_size = gfx::Size(default_tile_size, default_tile_size);
  if (cmd.HasSwitch(switches::kDefaultTileWidth)) {
    int tile_width = 0;
    switch_value_as_int(cmd, switches::kDefaultTileWidth, 1,
                        std::numeric_limits<int>::max(), &tile_width);
    settings.default_tile_size.set_width(tile_width);
  }
  if (cmd.HasSwitch(switches::kDefaultTileHeight)) {
    int tile_height = 0;
    switch_value_as_int(cmd, switches::kDefaultTileHeight, 1,
                        std::numeric_limits<int>::max(), &tile_height);
    settings.default_tile_size.set_height(tile_height);
  }

  if (cmd.HasSwitch(switches::kMinHeightForGpuRasterTile)) {
    int min_height_for_gpu_raster_tile = 0;
    switch_value_as_int(cmd, switches::kMinHeightForGpuRasterTile, 1,
                        std::numeric_limits<int>::max(),
                        &min_height_for_gpu_raster_tile);
    settings.min_height_for_gpu_raster_tile = min_height_for_gpu_raster_tile;
  }

  int max_untiled_layer_width = settings.max_untiled_layer_size.width();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerWidth)) {
    switch_value_as_int(cmd, switches::kMaxUntiledLayerWidth, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_width);
  }
  int max_untiled_layer_height = settings.max_untiled_layer_size.height();
  if (cmd.HasSwitch(switches::kMaxUntiledLayerHeight)) {
    switch_value_as_int(cmd, switches::kMaxUntiledLayerHeight, 1,
                        std::numeric_limits<int>::max(),
                        &max_untiled_layer_height);
  }

  settings.max_untiled_layer_size =
      gfx::Size(max_untiled_layer_width, max_untiled_layer_height);

  int gpu_rasterization_msaa_sample_count = -1;
  if (cmd.HasSwitch(switches::kGpuRasterizationMSAASampleCount)) {
    std::string string_value =
        cmd.GetSwitchValueASCII(switches::kGpuRasterizationMSAASampleCount);
    bool parsed_msaa_sample_count =
        base::StringToInt(string_value, &gpu_rasterization_msaa_sample_count);
    DCHECK(parsed_msaa_sample_count) << string_value;
    DCHECK_GE(gpu_rasterization_msaa_sample_count, 0);
  }
  settings.gpu_rasterization_msaa_sample_count =
      gpu_rasterization_msaa_sample_count;

  settings.can_use_lcd_text = platform->IsLcdTextEnabled();
  settings.use_zero_copy = cmd.HasSwitch(switches::kEnableZeroCopy);
  settings.use_partial_raster = !cmd.HasSwitch(switches::kDisablePartialRaster);
  // Partial raster is not supported with RawDraw
  settings.use_partial_raster &= !::features::IsUsingRawDraw();
  settings.enable_elastic_overscroll = platform->IsElasticOverscrollEnabled();
  settings.use_gpu_memory_buffer_resources =
      cmd.HasSwitch(switches::kEnableGpuMemoryBufferCompositorResources);
  settings.use_painted_device_scale_factor = true;

  // Build LayerTreeSettings from command line args.
  if (cmd.HasSwitch(cc::switches::kBrowserControlsShowThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kBrowserControlsShowThreshold);
    double show_threshold;
    if (base::StringToDouble(top_threshold_str, &show_threshold) &&
        show_threshold >= 0.f && show_threshold <= 1.f)
      settings.top_controls_show_threshold = show_threshold;
  }

  if (cmd.HasSwitch(cc::switches::kBrowserControlsHideThreshold)) {
    std::string top_threshold_str =
        cmd.GetSwitchValueASCII(cc::switches::kBrowserControlsHideThreshold);
    double hide_threshold;
    if (base::StringToDouble(top_threshold_str, &hide_threshold) &&
        hide_threshold >= 0.f && hide_threshold <= 1.f)
      settings.top_controls_hide_threshold = hide_threshold;
  }

  // Blink sends cc a layer list and property trees.
  settings.use_layer_lists = true;

  // Blink currently doesn't support setting fractional scroll offsets so CC
  // must send integer values. We plan to eventually make Blink use fractional
  // offsets internally: https://crbug.com/414283.
  settings.commit_fractional_scroll_deltas =
      RuntimeEnabledFeatures::FractionalScrollOffsetsEnabled();

  settings.enable_smooth_scroll = platform->IsScrollAnimatorEnabled();

  // The means the renderer compositor has 2 possible modes:
  // - Threaded compositing with a scheduler.
  // - Single threaded compositing without a scheduler (for web tests only).
  // Using the scheduler in web tests introduces additional composite steps
  // that create flakiness.
  settings.single_thread_proxy_scheduler = false;

  // These flags should be mirrored by UI versions in ui/compositor/.
  if (cmd.HasSwitch(cc::switches::kShowCompositedLayerBorders))
    settings.initial_debug_state.show_debug_borders.set();
  settings.initial_debug_state.show_fps_counter =
      cmd.HasSwitch(cc::switches::kShowFPSCounter);
  settings.initial_debug_state.show_layer_animation_bounds_rects =
      cmd.HasSwitch(cc::switches::kShowLayerAnimationBounds);
  settings.initial_debug_state.show_paint_rects =
      cmd.HasSwitch(switches::kShowPaintRects);
  settings.initial_debug_state.show_layout_shift_regions =
      cmd.HasSwitch(switches::kShowLayoutShiftRegions);
  settings.initial_debug_state.show_property_changed_rects =
      cmd.HasSwitch(cc::switches::kShowPropertyChangedRects);
  settings.initial_debug_state.show_surface_damage_rects =
      cmd.HasSwitch(cc::switches::kShowSurfaceDamageRects);
  settings.initial_debug_state.show_screen_space_rects =
      cmd.HasSwitch(cc::switches::kShowScreenSpaceRects);
  settings.initial_debug_state.highlight_non_lcd_text_layers =
      cmd.HasSwitch(cc::switches::kHighlightNonLCDTextLayers);

  settings.initial_debug_state.SetRecordRenderingStats(
      cmd.HasSwitch(cc::switches::kEnableGpuBenchmarking));

  if (cmd.HasSwitch(cc::switches::kSlowDownRasterScaleFactor)) {
    const int kMinSlowDownScaleFactor = 0;
    const int kMaxSlowDownScaleFactor = INT_MAX;
    switch_value_as_int(
        cmd, cc::switches::kSlowDownRasterScaleFactor, kMinSlowDownScaleFactor,
        kMaxSlowDownScaleFactor,
        &settings.initial_debug_state.slow_down_raster_scale_factor);
  }

  settings.scrollbar_animator = cc::LayerTreeSettings::ANDROID_OVERLAY;

  InitializeScrollbarFadeAndDelay(settings);

  if (cmd.HasSwitch(cc::switches::kCCScrollAnimationDurationForTesting)) {
    const int kMinScrollAnimationDuration = 0;
    const int kMaxScrollAnimationDuration = INT_MAX;
    int duration;
    if (switch_value_as_int(cmd,
                            cc::switches::kCCScrollAnimationDurationForTesting,
                            kMinScrollAnimationDuration,
                            kMaxScrollAnimationDuration, &duration)) {
      settings.scroll_animation_duration_for_testing = base::Seconds(duration);
    }
  }

#if BUILDFLAG(IS_ANDROID)
  // Synchronous compositing is used only for the outermost main frame.
  bool use_synchronous_compositor =
      platform->IsSynchronousCompositingEnabledForAndroidWebView() &&
      !is_for_embedded_frame;
  // Do not use low memory policies for Android WebView.
  bool using_low_memory_policy =
      base::SysInfo::IsLowEndDevice() && !IsSmallScreen(screen_size) &&
      !platform->IsSynchronousCompositingEnabledForAndroidWebView();

  settings.use_stream_video_draw_quad = true;
  settings.using_synchronous_renderer_compositor = use_synchronous_compositor;
  if (use_synchronous_compositor) {
    // Root frame in Android WebView uses system scrollbars, so make ours
    // invisible. http://crbug.com/677348: This can't be done using
    // hide_scrollbars setting because supporting -webkit custom scrollbars is
    // still desired on sublayers.
    settings.scrollbar_animator = cc::LayerTreeSettings::NO_ANIMATOR;
    // Rendering of scrollbars will be disabled in cc::SolidColorScrollbarLayer.

    // Early damage check works in combination with synchronous compositor.
    settings.enable_early_damage_check =
        cmd.HasSwitch(cc::switches::kCheckDamageEarly);
  }
  if (using_low_memory_policy) {
    // On low-end we want to be very careful about killing other
    // apps. So initially we use 50% more memory to avoid flickering
    // or raster-on-demand.
    settings.max_memory_for_prepaint_percentage = 67;
  } else {
    // On other devices we have increased memory excessively to avoid
    // raster-on-demand already, so now we reserve 50% _only_ to avoid
    // raster-on-demand, and use 50% of the memory otherwise.
    settings.max_memory_for_prepaint_percentage = 50;
  }

  // TODO(danakj): Only do this on low end devices.
  settings.create_low_res_tiling = true;

#else   // BUILDFLAG(IS_ANDROID)
  const bool using_low_memory_policy = base::SysInfo::IsLowEndDevice();

  settings.enable_fluent_scrollbar = ui::IsFluentScrollbarEnabled();
  settings.enable_fluent_overlay_scrollbar =
      ui::IsFluentOverlayScrollbarEnabled();

  if (ui::IsOverlayScrollbarEnabled()) {
    settings.scrollbar_animator = cc::LayerTreeSettings::AURA_OVERLAY;
    settings.scrollbar_thinning_duration =
        ui::kOverlayScrollbarThinningDuration;
    settings.scrollbar_flash_after_any_scroll_update =
        !settings.enable_fluent_overlay_scrollbar;
    // Avoid animating in web tests to improve reliability.
    if (settings.enable_fluent_overlay_scrollbar) {
      settings.scrollbar_thinning_duration =
          ui::kFluentOverlayScrollbarThinningDuration;
      if (WebTestSupport::IsRunningWebTest()) {
        settings.scrollbar_thinning_duration = base::Milliseconds(0);
        settings.scrollbar_fade_delay = base::Milliseconds(0);
        settings.scrollbar_fade_duration = base::Milliseconds(0);
      }
    }
  }
#endif  // BUILDFLAG(IS_ANDROID)

  settings.decoded_image_working_set_budget_bytes =
      cc::ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
          /*for_renderer=*/true);

  if (using_low_memory_policy) {
    // RGBA_4444 textures are only enabled:
    //  - If the user hasn't explicitly disabled them
    //  - If system ram is <= 512MB (1GB devices are sometimes low-end).
    //  - If we are not running in a WebView, where 4444 isn't supported.
    //  - If we are not using vulkan, since some GPU drivers don't support
    //    using RGBA4444 as color buffer.
    // TODO(penghuang): query supported formats from GPU process.
    if (!cmd.HasSwitch(switches::kDisableRGBA4444Textures) &&
        base::SysInfo::AmountOfPhysicalMemoryMB() <= 512 &&
        !::features::IsUsingVulkan()) {
      settings.use_rgba_4444 = true;

      // If we are going to unpremultiply and dither these tiles, we need to
      // allocate an additional RGBA_8888 intermediate for each tile
      // rasterization when rastering to RGBA_4444 to allow for dithering.
      // Setting a reasonable sized max tile size allows this intermediate to
      // be consistently reused.
      if (base::FeatureList::IsEnabled(
              kUnpremultiplyAndDitherLowBitDepthTiles)) {
        settings.max_gpu_raster_tile_size = gfx::Size(512, 256);
        settings.unpremultiply_and_dither_low_bit_depth_tiles = true;
      }
    }
  }

  if (cmd.HasSwitch(switches::kEnableLowResTiling))
    settings.create_low_res_tiling = true;
  if (cmd.HasSwitch(switches::kDisableLowResTiling))
    settings.create_low_res_tiling = false;

  if (cmd.HasSwitch(switches::kEnableRGBA4444Textures) &&
      !cmd.HasSwitch(switches::kDisableRGBA4444Textures)) {
    settings.use_rgba_4444 = true;
  }

  settings.max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;  // 32MB
  // Use 1/4th of staging buffers on low-end devices.
  if (base::SysInfo::IsLowEndDevice())
    settings.max_staging_buffer_usage_in_bytes /= 4;

  cc::ManagedMemoryPolicy defaults = settings.memory_policy;
  settings.memory_policy = GetGpuMemoryPolicy(defaults, initial_screen_size,
                                              initial_device_scale_factor);

  settings.disallow_non_exact_resource_reuse =
      cmd.HasSwitch(::switches::kDisallowNonExactResourceReuse);
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/746931): This feature appears to be causing visual
  // corruption on certain android devices. Will investigate and re-enable.
  settings.disallow_non_exact_resource_reuse = true;
#endif

  settings.wait_for_all_pipeline_stages_before_draw =
      cmd.HasSwitch(::switches::kRunAllCompositorStagesBeforeDraw);

  settings.enable_image_animation_resync =
      !cmd.HasSwitch(switches::kDisableImageAnimationResync);

  settings.enable_backface_visibility_interop =
      RuntimeEnabledFeatures::BackfaceVisibilityInteropEnabled();

  settings.disable_frame_rate_limit =
      cmd.HasSwitch(::switches::kDisableFrameRateLimit);

  settings.enable_hit_test_opaqueness =
      RuntimeEnabledFeatures::HitTestOpaquenessEnabled();

  settings.enable_variable_refresh_rate =
      ::features::IsVariableRefreshRateAlwaysOn();

  std::tie(settings.tiling_interest_area_padding,
           settings.skewport_extrapolation_limit_in_screen_pixels) =
      GetTilingInterestAreaSizes();
  return settings;
}

}  // namespace blink
