// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/switches.h"

namespace blink {
namespace switches {

// Allows processing of input before a frame has been committed.
// TODO(crbug.com/987626): Used by headless. Look for a way not
// involving a command line switch.
const char kAllowPreCommitInput[] = "allow-pre-commit-input";

// Set blink settings. Format is <name>[=<value],<name>[=<value>],...
// The names are declared in Settings.json5. For boolean type, use "true",
// "false", or omit '=<value>' part to set to true. For enum type, use the int
// value of the enum value. Applied after other command line flags and prefs.
const char kBlinkSettings[] = "blink-settings";

// Sets dark mode settings. Format is [<param>=<value>],[<param>=<value>],...
// The params take either int or float values. If params are not specified,
// the default dark mode settings is used. Valid params are given below.
// "InversionAlgorithm" takes int value of DarkModeInversionAlgorithm enum.
// "ImagePolicy" takes int value of DarkModeImagePolicy enum.
// "ForegroundBrightnessThreshold" takes 0 to 255 int value.
// "BackgroundBrightnessThreshold" takes 0 to 255 int value.
// "ContrastPercent" takes -1.0 to 1.0 float value. Higher the value, more
// the contrast.
const char kDarkModeSettings[] = "dark-mode-settings";

// Overrides data: URLs in SVGUseElement deprecation through enterprise policy.
const char kDataUrlInSvgUseEnabled[] = "data-url-in-svg-use-enabled";

// Sets the tile size used by composited layers.
const char kDefaultTileWidth[] = "default-tile-width";
const char kDefaultTileHeight[] = "default-tile-height";

// Disallow image animations to be reset to the beginning to avoid skipping
// many frames. Only effective if compositor image animations are enabled.
const char kDisableImageAnimationResync[] = "disable-image-animation-resync";

// When using CPU rasterizing disable low resolution tiling. This uses
// less power, particularly during animations, but more white may be seen
// during fast scrolling especially on slower devices.
const char kDisableLowResTiling[] = "disable-low-res-tiling";

// Disallow use of the feature NewBaseUrlInheritanceBehavior.
const char kDisableNewBaseUrlInheritanceBehavior[] =
    "disable-new-base-url-inheritance-behavior";

// Disable partial raster in the renderer. Disabling this switch also disables
// the use of persistent gpu memory buffers.
const char kDisablePartialRaster[] = "disable-partial-raster";

// Disable the creation of compositing layers when it would prevent LCD text.
const char kDisablePreferCompositingToLCDText[] =
    "disable-prefer-compositing-to-lcd-text";

// Disables RGBA_4444 textures.
const char kDisableRGBA4444Textures[] = "disable-rgba-4444-textures";

// Disable multithreaded, compositor scrolling of web content.
const char kDisableThreadedScrolling[] = "disable-threaded-scrolling";

// Disable rasterizer that writes directly to GPU memory associated with tiles.
const char kDisableZeroCopy[] = "disable-zero-copy";

// Logs Runtime Call Stats. --single-process also needs to be used along with
// this for the stats to be logged.
const char kDumpRuntimeCallStats[] = "dump-blink-runtime-call-stats";

// Specify that all compositor resources should be backed by GPU memory buffers.
const char kEnableGpuMemoryBufferCompositorResources[] =
    "enable-gpu-memory-buffer-compositor-resources";

// When using CPU rasterizing generate low resolution tiling. Low res
// tiles may be displayed during fast scrolls especially on slower devices.
const char kEnableLowResTiling[] = "enable-low-res-tiling";

// Enable the creation of compositing layers when it would prevent LCD text.
const char kEnablePreferCompositingToLCDText[] =
    "enable-prefer-compositing-to-lcd-text";

// Enables RGBA_4444 textures.
const char kEnableRGBA4444Textures[] = "enable-rgba-4444-textures";

// Enables raster side dark mode for images.
const char kEnableRasterSideDarkModeForImages[] =
    "enable-raster-side-dark-mode-for-images";

// Enable rasterizer that writes directly to GPU memory associated with tiles.
const char kEnableZeroCopy[] = "enable-zero-copy";

// The number of multisample antialiasing samples for GPU rasterization.
// Requires MSAA support on GPU to have an effect. 0 disables MSAA.
const char kGpuRasterizationMSAASampleCount[] =
    "gpu-rasterization-msaa-sample-count";

// Used to communicate managed policy for the IntensiveWakeUpThrottling feature.
// This feature is typically controlled by base::Feature (see
// renderer/platform/scheduler/common/features.*) but requires an enterprise
// policy override. This is implicitly a tri-state, and can be either unset, or
// set to "1" for force enable, or "0" for force disable.
extern const char kIntensiveWakeUpThrottlingPolicy[] =
    "intensive-wake-up-throttling-policy";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceDisable[] = "0";
extern const char kIntensiveWakeUpThrottlingPolicy_ForceEnable[] = "1";

// Sets the width and height above which a composited layer will get tiled.
const char kMaxUntiledLayerHeight[] = "max-untiled-layer-height";
const char kMaxUntiledLayerWidth[] = "max-untiled-layer-width";

// Sets the min tile height for GPU raster.
const char kMinHeightForGpuRasterTile[] = "min-height-for-gpu-raster-tile";

// Sets the timeout seconds of the network-quiet timers in IdlenessDetector.
// Used by embedders who want to change the timeout time in order to run web
// contents on various embedded devices and changeable network bandwidths in
// different regions. For example, it's useful when using FirstMeaningfulPaint
// signal to dismiss a splash screen.
const char kNetworkQuietTimeout[] = "network-quiet-timeout";

// Visibly render a border around layout shift rects in the web page to help
// debug and study layout shifts.
const char kShowLayoutShiftRegions[] = "show-layout-shift-regions";

// Visibly render a border around paint rects in the web page to help debug
// and study painting behavior.
const char kShowPaintRects[] = "show-paint-rects";

// Used to override the ThrottleDisplayNoneAndVisibilityHiddenCrossOrigin
// feature from an enterprise policy.
const char kDisableThrottleNonVisibleCrossOriginIframes[] =
    "disable-throttle-non-visible-cross-origin-iframes";

// Controls how text selection granularity changes when touch text selection
// handles are dragged. Should be "character" or "direction". If not specified,
// the platform default is used.
const char kTouchTextSelectionStrategy[] = "touch-selection-strategy";
const char kTouchTextSelectionStrategy_Character[] = "character";
const char kTouchTextSelectionStrategy_Direction[] = "direction";

// Comma-separated list of origins that can use SharedArrayBuffer without
// enabling cross-origin isolation.
const char kSharedArrayBufferAllowedOrigins[] =
    "shared-array-buffer-allowed-origins";

// Allows overriding the conditional focus window's length.
const char kConditionalFocusWindowMs[] = "conditional-focus-window-ms";

// Specifies the flags passed to JS engine.
const char kJavaScriptFlags[] = "js-flags";

// Controls whether WebSQL is force enabled.
const char kWebSQLAccess[] = "web-sql-access";

// Used to communicate managed policy for the EventPath feature. This feature is
// typically controlled by base::Feature (see blink/common/features.*) but
// requires an enterprise policy override. This is implicitly a tri-state, and
// can be either unset, or set to "1" for force enable, or "0" for force
// disable.
extern const char kEventPathPolicy[] = "event-path-policy";
extern const char kEventPathPolicy_ForceDisable[] = "0";
extern const char kEventPathPolicy_ForceEnable[] = "1";

// The EventPath feature is disabled by default on almost all platforms and
// channels, with a few exceptions that require a more gradual removal. Those
// platforms/channels should pass this flag to renderer to enable the feature.
// The flag has higher precedence than Blink runtime enabled feature, but lower
// precedence than base::Feature overrides and enterprise policy.
extern const char kEventPathEnabledByDefault[] =
    "event-path-enabled-by-default";

// Used to communicate managed policy for the OffsetParentNewSpecBehavior
// feature. This feature is typically controlled by base::Feature (see
// blink/common/features.*) but requires an enterprise policy override. This is
// implicitly a tri-state, and can be either unset, or set to "1" for force
// enable, or "0" for force disable.
extern const char kOffsetParentNewSpecBehaviorPolicy[] =
    "offset-parent-new-spec-behavior-policy";
extern const char kOffsetParentNewSpecBehaviorPolicy_ForceDisable[] = "0";
extern const char kOffsetParentNewSpecBehaviorPolicy_ForceEnable[] = "1";

// Used to communicate managed policy for the
// SendMouseEventsDisabledFormControls feature. This feature is typically
// controlled by base::Feature (see blink/common/features.*) but requires an
// enterprise policy override. This is implicitly a tri-state, and can be either
// unset, or set to "1" for force enable, or "0" for force disable.
extern const char kSendMouseEventsDisabledFormControlsPolicy[] =
    "send-mouse-events-disabled-form-controls-policy";
extern const char kSendMouseEventsDisabledFormControlsPolicy_ForceDisable[] =
    "0";
extern const char kSendMouseEventsDisabledFormControlsPolicy_ForceEnable[] =
    "1";

}  // namespace switches
}  // namespace blink
