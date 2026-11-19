// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/privacy_overlay_mac.h"

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "remoting/base/string_resources.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Private WindowServer SPI used across Chromium
// (`native_widget_ns_window_bridge.mm`) to immediately exclude a window's
// region from WindowServer / ScreenCaptureKit desktop capture in addition to
// `NSWindowSharingNone`.
using CGSConnectionID = uint32_t;
using CGSWindowID = NSInteger;
using CGRegionRef = CFTypeRef;

CG_EXTERN CGSConnectionID CGSMainConnectionID(void);
CG_EXTERN CGError CGSSetWindowCaptureExcludeShape(CGSConnectionID cid,
                                                  CGSWindowID wid,
                                                  CGRegionRef region);
CG_EXTERN CGRegionRef CGRegionCreateWithRect(CGRect rect);

namespace {

// Branded Google Blue (`#1A73E8`) components used when Increase Contrast mode
// is not active.
constexpr CGFloat kGoogleBlueRed = 26.0 / 255.0;
constexpr CGFloat kGoogleBlueGreen = 115.0 / 255.0;
constexpr CGFloat kGoogleBlueBlue = 232.0 / 255.0;

// Layout constants (in Cocoa points) for the privacy overlay backdrop and
// floating Disconnect button window.
constexpr CGFloat kIconSize = 64.0;
constexpr CGFloat kTitleFontSize = 21.0;
constexpr CGFloat kBodyFontSize = 14.0;
constexpr CGFloat kVerticalSpacing = 16.0;
constexpr CGFloat kButtonTopOffset = 72.0;
constexpr CGFloat kMaxTextWidthRatio = 0.8;

constexpr CGFloat kButtonCornerRadius = 14.0;
constexpr CGFloat kButtonBorderWidth = 2.0;
constexpr CGFloat kButtonMargin = 12.0;
constexpr CGFloat kButtonScreenHorizontalMargin = 16.0;
constexpr CGFloat kMinButtonWidth = 100.0;
constexpr CGFloat kMinButtonHeight = 24.0;

// Baseline macOS desktop `Body` text-style size (13pt for `DEFAULT` / `M`) and
// supported clamp bounds for the user's Accessibility text scale factor
// ("System Settings -> Accessibility -> Display -> Text size").
constexpr CGFloat kDefaultMacBodyFontSizePt = 13.0;
constexpr CGFloat kMinTextScaleFactor = 1.0;
constexpr CGFloat kMaxTextScaleFactor = 2.5;

NSString* const kUniversalAccessSuiteName = @"com.apple.universalaccess";
NSString* const kFontSizeCategoryKey = @"FontSizeCategory";
NSString* const kFontSizeUseGlobalValue = @"UseGlobal";
NSString* const kFontSizeGlobalKey = @"global";

// Per-axis movement threshold (in integer points truncated from
// `CGEventGetLocation`, i.e., Chebyshev distance) required before local
// pointer activity reveals the Disconnect button, filtering out <= 2pt
// hardware/software cursor drift across both Retina and non-Retina (1x)
// displays (including diagonal `(2, 2)` moves) while remaining well below
// intentional user movement. Because remote-injected events are filtered
// upstream, `mouse_position_` tracks the last *local* cursor position; if the
// remote user moves the shared cursor first, a subsequent local hardware event
// at the new location exceeds this threshold and reveals the button (acceptable
// since the event originates from genuine local hardware input).
constexpr int kMouseJitterThresholdPt = 2;

// The amount of time to wait before hiding the Disconnect button after local
// input stops.
constexpr base::TimeDelta kAutoHideTimeout = base::Seconds(10);

remoting::PrivacyOverlayMac::ScreenFramesProvider&
GetScreenFramesProviderStorage() {
  static base::NoDestructor<remoting::PrivacyOverlayMac::ScreenFramesProvider>
      provider;
  return *provider;
}

remoting::PrivacyOverlayMac::CaptureExclusionApplier&
GetCaptureExclusionApplierStorage() {
  static base::NoDestructor<
      remoting::PrivacyOverlayMac::CaptureExclusionApplier>
      applier;
  return *applier;
}

remoting::PrivacyOverlayMac::MouseLocationProvider&
GetMouseLocationProviderStorage() {
  static base::NoDestructor<remoting::PrivacyOverlayMac::MouseLocationProvider>
      provider;
  return *provider;
}

struct UniversalAccessTestingOverride {
  bool active = false;
  NSUserDefaults* __strong defaults = nil;
  NSString* __strong bundle_id = nil;
};

UniversalAccessTestingOverride& GetUniversalAccessTestingOverrideStorage() {
  static base::NoDestructor<UniversalAccessTestingOverride> storage;
  return *storage;
}

// Maps a macOS Accessibility `FontSizeCategory` tier name (under
// `com.apple.universalaccess`) to its empirically measured desktop `Body` point
// size on macOS 14 (Sonoma) and macOS 15 (Sequoia). Unlike iOS Dynamic Type
// (where `UIContentSizeCategoryLarge` `Body` defaults to 17pt and scales up to
// `AX5` = 53pt), macOS uses a 13pt desktop baseline (`DEFAULT` / `M` = 13pt)
// with slider steps `L` = 14pt, `XL` = 15pt, `XXL` = 16pt, `XXXL` = 17pt,
// `AX1` = 20pt, `AX2` = 24pt, `AX3` = 29pt, `AX4` = 35pt, and `AX5` = 42pt.
// Returns `kDefaultMacBodyFontSizePt` (13pt) for `DEFAULT`, `M`, unrecognized
// values, or tiers smaller than 13pt (`XXXS`–`S`) so privacy overlay text is
// never shrunk below baseline.
CGFloat PointSizeForFontSizeCategory(NSString* category) {
  if (!category || category.length == 0) {
    return kDefaultMacBodyFontSizePt;
  }
  if ([category isEqualToString:@"L"]) {
    return 14.0;
  }
  if ([category isEqualToString:@"XL"]) {
    return 15.0;
  }
  if ([category isEqualToString:@"XXL"]) {
    return 16.0;
  }
  if ([category isEqualToString:@"XXXL"]) {
    return 17.0;
  }
  if ([category isEqualToString:@"AX1"]) {
    return 20.0;
  }
  if ([category isEqualToString:@"AX2"]) {
    return 24.0;
  }
  if ([category isEqualToString:@"AX3"]) {
    return 29.0;
  }
  if ([category isEqualToString:@"AX4"]) {
    return 35.0;
  }
  if ([category isEqualToString:@"AX5"]) {
    return 42.0;
  }
  return kDefaultMacBodyFontSizePt;
}

CGFloat TextScaleFactorForCategory(NSString* category) {
  return std::clamp(
      PointSizeForFontSizeCategory(category) / kDefaultMacBodyFontSizePt,
      kMinTextScaleFactor, kMaxTextScaleFactor);
}

NSUserDefaults* CreateUniversalAccessDefaults() {
  const auto& override_state = GetUniversalAccessTestingOverrideStorage();
  if (override_state.active) {
    return override_state.defaults;
  }
  return [[NSUserDefaults alloc] initWithSuiteName:kUniversalAccessSuiteName];
}

// Queries the active user's Accessibility text scale factor from
// `com.apple.universalaccess` / `FontSizeCategory` (clamped to `[1.0, 2.5]`).
//
// Pure AppKit font APIs (`NSFont.systemFontSize` and
// `NSFontDescriptor.preferredFontDescriptorForTextStyle:`) remain fixed at 13pt
// on macOS and do not track *System Settings -> Accessibility -> Display ->
// Text size*. `com.apple.universalaccess` / `FontSizeCategory` is an
// undocumented private preference introduced in macOS 14 (Sonoma) and validated
// against macOS 14 (Sonoma) and macOS 15 (Sequoia): unsandboxed user-session
// processes (such as `remoting_me2me_host`, which runs as a LaunchAgent with
// Accessibility and Screen Recording TCC grants) read the domain via
// `cfprefsd`, whereas direct plist file reads under `~/Library/Preferences/`
// are TCC-protected since macOS 10.14. If `universal_access_defaults` or
// `FontSizeCategory` is absent or empty (e.g. on macOS 13 Ventura, under
// stricter TCC confinement, or if a future macOS release changes the schema),
// this gracefully returns `kMinTextScaleFactor` (1.0).
CGFloat GetSystemTextScaleFactor(NSUserDefaults* universal_access_defaults) {
  if (!universal_access_defaults) {
    return kMinTextScaleFactor;
  }

  NSDictionary* font_size_dict = base::apple::ObjCCast<NSDictionary>(
      [universal_access_defaults objectForKey:kFontSizeCategoryKey]);
  if (!font_size_dict) {
    return kMinTextScaleFactor;
  }

  const auto& override_state = GetUniversalAccessTestingOverrideStorage();
  NSString* bundle_id = override_state.active
                            ? override_state.bundle_id
                            : NSBundle.mainBundle.bundleIdentifier;
  NSString* category = nil;
  if (bundle_id.length > 0) {
    NSString* app_category =
        base::apple::ObjCCast<NSString>(font_size_dict[bundle_id]);
    if (app_category.length > 0 &&
        ![app_category isEqualToString:kFontSizeUseGlobalValue]) {
      category = app_category;
    }
  }
  if (!category) {
    category =
        base::apple::ObjCCast<NSString>(font_size_dict[kFontSizeGlobalKey]);
  }
  return TextScaleFactorForCategory(category);
}

CGFloat ScaleFontSize(CGFloat base_font_size, CGFloat text_scale) {
  CGFloat clamped =
      std::clamp(text_scale, kMinTextScaleFactor, kMaxTextScaleFactor);
  return std::round(base_font_size * clamped);
}

// Returns active display frames in Cocoa global screen coordinates. Both
// `NSScreen.screens` and `ScreenFramesProvider` guarantee that index 0 (when
// non-empty) is the primary (menu-bar) display used as the vertical reference
// when converting between Cocoa and Quartz global display coordinates.
std::vector<NSRect> GetScreenFrames() {
  const auto& provider = GetScreenFramesProviderStorage();
  if (provider) {
    return provider.Run();
  }
  NSArray<NSScreen*>* screens = NSScreen.screens;
  std::vector<NSRect> frames;
  frames.reserve(screens.count);
  for (NSScreen* screen in screens) {
    frames.push_back(screen.frame);
  }
  return frames;
}

NSPoint GetCurrentMouseLocation() {
  const auto& provider = GetMouseLocationProviderStorage();
  if (provider) {
    return provider.Run();
  }
  return NSEvent.mouseLocation;
}

bool ApplyCaptureExclusionForSize(NSWindow* window, NSSize size) {
  if (!window || size.width <= 0.0 || size.height <= 0.0) {
    LOG(ERROR) << "Invalid window or size (" << size.width << "x" << size.height
               << ") for capture exclusion";
    return false;
  }

  window.sharingType = NSWindowSharingNone;

  const auto& applier = GetCaptureExclusionApplierStorage();
  if (applier) {
    return applier.Run(window, size);
  }

  CGSConnectionID connection_id = CGSMainConnectionID();
  CGSWindowID window_id = window.windowNumber;
  if (connection_id == 0 || window_id <= 0) {
    LOG(ERROR) << "Invalid WindowServer identifiers for capture exclusion "
               << "(connection_id=" << connection_id
               << ", window_id=" << window_id << ")";
    return false;
  }

  CGRect rect = CGRectMake(0.0, 0.0, size.width, size.height);
  base::apple::ScopedCFTypeRef<CGRegionRef> region(
      CGRegionCreateWithRect(rect));
  if (!region) {
    LOG(ERROR) << "CGRegionCreateWithRect failed for size " << size.width << "x"
               << size.height;
    return false;
  }

  CGError error =
      CGSSetWindowCaptureExcludeShape(connection_id, window_id, region.get());
  if (error != kCGErrorSuccess) {
    LOG(ERROR) << "CGSSetWindowCaptureExcludeShape failed with CGError "
               << error;
    return false;
  }

  return true;
}

bool ApplyCaptureExclusion(NSWindow* window) {
  if (!window) {
    return false;
  }
  return ApplyCaptureExclusionForSize(window, window.frame.size);
}

bool IsUserInputEventType(NSEventType type) {
  switch (type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeScrollWheel:
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
      return true;
    // `NSEventTypeFlagsChanged` is intentionally excluded (consistent with
    // `LocalKeyboardInputMonitorMac`, crbug.com/578661739): modifier-flag
    // transitions cannot activate `_disconnectButton`, and WindowServer
    // synthesizes `FlagsChanged` notifications during application switches.
    default:
      return false;
  }
}

bool IsRemoteInjectedEvent(NSEvent* event) {
  if (!event || !IsUserInputEventType(event.type)) {
    return false;
  }
  CGEventRef cg_event = event.CGEvent;
  if (!cg_event) {
    return false;
  }
  // Events synthesized by `InputInjectorMac` originate from the current CRD
  // host process (`kCGEventSourceUnixProcessID == getpid()`), whereas physical
  // hardware events have `pid == 0` (matching `LocalMouseInputMonitorMac`).
  int64_t source_pid =
      CGEventGetIntegerValueField(cg_event, kCGEventSourceUnixProcessID);
  return source_pid == static_cast<int64_t>(getpid());
}

// Converts a point in Quartz global display coordinates (`(0, 0)` at top-left
// of the primary screen, `y` increasing downward) to Cocoa screen coordinates
// (`(0, 0)` at bottom-left of the primary screen, `y` increasing upward).
// `screen_frames.front()` is the primary (menu-bar) display per
// `GetScreenFrames()`.
NSPoint ConvertQuartzPointToCocoa(const std::vector<NSRect>& screen_frames,
                                  const webrtc::DesktopVector& position) {
  CGFloat primary_height =
      screen_frames.empty() ? 0.0 : NSHeight(screen_frames.front());
  return NSMakePoint(position.x(), primary_height - position.y());
}

// Converts a point in Cocoa screen coordinates to Quartz global display
// coordinates using the primary (menu-bar) display at `screen_frames.front()`.
webrtc::DesktopVector ConvertCocoaPointToQuartz(
    const std::vector<NSRect>& screen_frames,
    NSPoint cocoa_point) {
  CGFloat primary_height =
      screen_frames.empty() ? 0.0 : NSHeight(screen_frames.front());
  return webrtc::DesktopVector(
      static_cast<int32_t>(std::lround(cocoa_point.x)),
      static_cast<int32_t>(std::lround(primary_height - cocoa_point.y)));
}

size_t FindScreenIndexForCocoaPoint(const std::vector<NSRect>& screen_frames,
                                    NSPoint point) {
  // `NSMouseInRect(..., /*flipped=*/NO)` handles Cocoa's bottom-left origin so
  // cursor coordinates on the top row of a display (`y == NSMaxY(frame)`) match
  // that display rather than falling through.
  for (size_t i = 0; i < screen_frames.size(); ++i) {
    if (NSMouseInRect(point, screen_frames[i], /*flipped=*/NO)) {
      return i;
    }
  }

  // Fall back to the nearest screen when `point` lies on an excluded boundary
  // (`y == NSMinY` or `x == NSMaxX`) or in a gap between displays.
  size_t nearest_index = 0;
  CGFloat best_distance_sq = std::numeric_limits<CGFloat>::infinity();
  for (size_t i = 0; i < screen_frames.size(); ++i) {
    const NSRect& frame = screen_frames[i];
    CGFloat clamped_x = std::clamp(point.x, NSMinX(frame), NSMaxX(frame));
    CGFloat clamped_y = std::clamp(point.y, NSMinY(frame), NSMaxY(frame));
    CGFloat dx = point.x - clamped_x;
    CGFloat dy = point.y - clamped_y;
    CGFloat distance_sq = dx * dx + dy * dy;
    if (distance_sq < best_distance_sq) {
      best_distance_sq = distance_sq;
      nearest_index = i;
    }
  }
  return nearest_index;
}

NSImage* LoadOverlayIcon() {
  NSBundle* class_bundle =
      [NSBundle bundleForClass:[PrivacyOverlayBackdropView class]];
  NSBundle* main_bundle = NSBundle.mainBundle;
  for (NSString* resource_name in @[ @"remoting_host", @"chromoting128" ]) {
    if (NSImage* icon = [class_bundle imageForResource:resource_name]) {
      return icon;
    }
    if (main_bundle != class_bundle) {
      if (NSImage* icon = [main_bundle imageForResource:resource_name]) {
        return icon;
      }
    }
  }
  return NSApp.applicationIconImage;
}

NSDictionary<NSAttributedStringKey, id>*
CreateTextAttributes(NSFont* font, NSColor* text_color, BOOL is_rtl) {
  NSMutableParagraphStyle* paragraph_style =
      [[NSMutableParagraphStyle alloc] init];
  paragraph_style.alignment = NSTextAlignmentCenter;
  paragraph_style.lineBreakMode = NSLineBreakByWordWrapping;
  paragraph_style.baseWritingDirection =
      is_rtl ? NSWritingDirectionRightToLeft : NSWritingDirectionLeftToRight;
  return @{
    NSFontAttributeName : font,
    NSForegroundColorAttributeName : text_color,
    NSParagraphStyleAttributeName : paragraph_style,
  };
}

CGFloat MeasureWrappedTextHeight(
    NSString* text,
    CGFloat max_width,
    NSDictionary<NSAttributedStringKey, id>* attributes,
    CGFloat min_height) {
  if (text.length == 0 || max_width <= 0.0) {
    return 0.0;
  }
  NSRect bounding_rect =
      [text boundingRectWithSize:NSMakeSize(max_width, CGFLOAT_MAX)
                         options:NSStringDrawingUsesLineFragmentOrigin |
                                 NSStringDrawingUsesFontLeading
                      attributes:attributes];
  return std::max(min_height, std::ceil(NSHeight(bounding_rect)));
}

struct BackdropRenderLayout {
  NSDictionary<NSAttributedStringKey, id>* __strong title_attrs;
  NSDictionary<NSAttributedStringKey, id>* __strong body_attrs;
  remoting::BackdropContentLayout rects;
};

BackdropRenderLayout ComputeBackdropRenderLayout(NSRect bounds,
                                                 NSString* title_text,
                                                 NSString* sharing_text,
                                                 NSColor* text_color,
                                                 BOOL is_rtl,
                                                 CGFloat text_scale) {
  BackdropRenderLayout layout;
  CGFloat width = NSWidth(bounds);
  CGFloat height = NSHeight(bounds);
  if (width <= 0.0 || height <= 0.0) {
    return layout;
  }

  CGFloat center_x = NSMidX(bounds);
  CGFloat center_y = NSMidY(bounds);
  CGFloat max_text_width =
      std::max<CGFloat>(1.0, std::floor(width * kMaxTextWidthRatio));
  CGFloat text_left = std::round(center_x - max_text_width / 2.0);

  const CGFloat title_font_size = ScaleFontSize(kTitleFontSize, text_scale);
  const CGFloat body_font_size = ScaleFontSize(kBodyFontSize, text_scale);

  NSFont* title_font = [NSFont systemFontOfSize:title_font_size
                                         weight:NSFontWeightSemibold];
  NSFont* body_font = [NSFont systemFontOfSize:body_font_size
                                        weight:NSFontWeightRegular];

  layout.title_attrs = CreateTextAttributes(title_font, text_color, is_rtl);
  layout.body_attrs = CreateTextAttributes(body_font, text_color, is_rtl);

  CGFloat title_height = MeasureWrappedTextHeight(
      title_text, max_text_width, layout.title_attrs, title_font_size);
  CGFloat body_height =
      MeasureWrappedTextHeight(sharing_text, max_text_width, layout.body_attrs,
                               body_font_size + kVerticalSpacing);

  // On short displays or at large text scales (up to 2.5x), shifting
  // `icon_top`, `title_bottom`, and `center_y` downward by the same
  // `shift_down` keeps the icon below `min_icon_top` while preserving the exact
  // `kVerticalSpacing` (16pt) separation between `icon_rect`, `title_rect`, and
  // `sharing_rect` so the three elements never overlap.
  const CGFloat min_icon_top = NSMinY(bounds) + kVerticalSpacing;
  CGFloat title_bottom = center_y - kVerticalSpacing;
  CGFloat icon_top = title_bottom - title_height - kVerticalSpacing - kIconSize;
  if (icon_top < min_icon_top) {
    CGFloat shift_down = min_icon_top - icon_top;
    icon_top = min_icon_top;
    title_bottom += shift_down;
    center_y += shift_down;
  }

  layout.rects.icon_rect =
      NSMakeRect(std::round(center_x - kIconSize / 2.0), std::round(icon_top),
                 kIconSize, kIconSize);
  layout.rects.title_rect =
      NSMakeRect(text_left, std::round(title_bottom - title_height),
                 max_text_width, title_height);
  layout.rects.sharing_rect =
      NSMakeRect(text_left, std::round(center_y), max_text_width, body_height);
  return layout;
}

}  // namespace

// Forwards AppKit/NSWorkspace notifications to `PrivacyOverlayMac`. Every
// notification callback retains `self` for its full lexical scope via
// `NS_VALID_UNTIL_END_OF_SCOPE` (either because the handler can invoke
// `EndOverlay()`, as `OnScreenParametersChanged()` does on failure, or for
// consistency against future edits to the other handlers): `EndOverlay()`
// invalidates and releases `notification_observer_` while `DisconnectSession()`
// may synchronously destroy `_overlay`, so `_overlay` must never be accessed
// after forwarding.
@interface PrivacyOverlayNotificationObserver : NSObject
- (instancetype)initWithOverlay:
                    (base::WeakPtr<remoting::PrivacyOverlayMac>)overlay
                   buttonWindow:(NSWindow*)buttonWindow;
- (void)invalidate;
@end

@implementation PrivacyOverlayNotificationObserver {
  base::WeakPtr<remoting::PrivacyOverlayMac> _overlay;
}

- (instancetype)initWithOverlay:
                    (base::WeakPtr<remoting::PrivacyOverlayMac>)overlay
                   buttonWindow:(NSWindow*)buttonWindow {
  self = [super init];
  if (self) {
    _overlay = std::move(overlay);
    [NSNotificationCenter.defaultCenter
        addObserver:self
           selector:@selector(onScreenParametersChanged:)
               name:NSApplicationDidChangeScreenParametersNotification
             object:nil];
    if (buttonWindow) {
      [NSNotificationCenter.defaultCenter
          addObserver:self
             selector:@selector(onButtonWindowDidMove:)
                 name:NSWindowDidMoveNotification
               object:buttonWindow];
    }
    [NSWorkspace.sharedWorkspace.notificationCenter
        addObserver:self
           selector:@selector(onAccessibilityDisplayOptionsChanged:)
               name:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
             object:nil];
  }
  return self;
}

- (void)dealloc {
  // Defensive safety net (`-invalidate` is idempotent): `EndOverlay()` normally
  // calls `-invalidate` explicitly before releasing this observer, and
  // `NSNotificationCenter` also auto-unregisters deallocated observers.
  [self invalidate];
}

- (void)invalidate {
  [NSNotificationCenter.defaultCenter removeObserver:self];
  [NSWorkspace.sharedWorkspace.notificationCenter removeObserver:self];
  _overlay.reset();
}

- (void)onScreenParametersChanged:(NSNotification*)notification {
  PrivacyOverlayNotificationObserver* keepAlive NS_VALID_UNTIL_END_OF_SCOPE =
      self;
  (void)keepAlive;
  if (_overlay) {
    _overlay->OnScreenParametersChanged();
  }
}

- (void)onButtonWindowDidMove:(NSNotification*)notification {
  PrivacyOverlayNotificationObserver* keepAlive NS_VALID_UNTIL_END_OF_SCOPE =
      self;
  (void)keepAlive;
  if (_overlay) {
    _overlay->OnButtonWindowDidMove();
  }
}

- (void)onAccessibilityDisplayOptionsChanged:(NSNotification*)notification {
  PrivacyOverlayNotificationObserver* keepAlive NS_VALID_UNTIL_END_OF_SCOPE =
      self;
  (void)keepAlive;
  if (_overlay) {
    _overlay->OnAccessibilityDisplayOptionsChanged();
  }
}

@end

@implementation PrivacyOverlayBackdropView {
  std::optional<bool> _highContrastOverrideForTesting;
}

@synthesize titleText = _titleText;
@synthesize sharingText = _sharingText;
@synthesize iconImage = _iconImage;
@synthesize isRTL = _isRTL;
@synthesize textScale = _textScale;

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
    _textScale = kMinTextScaleFactor;
    self.accessibilityElement = YES;
    self.accessibilityRole = NSAccessibilityStaticTextRole;
    self.userInterfaceLayoutDirection =
        NSUserInterfaceLayoutDirectionLeftToRight;
  }
  return self;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)isOpaque {
  return YES;
}

- (void)setFrameSize:(NSSize)newSize {
  NSSize oldSize = self.frame.size;
  [super setFrameSize:newSize];
  if (!NSEqualSizes(oldSize, self.frame.size)) {
    self.needsDisplay = YES;
  }
}

- (void)setTitleText:(NSString*)titleText {
  if (_titleText == titleText ||
      (titleText && [_titleText isEqualToString:titleText])) {
    return;
  }
  _titleText = [titleText copy];
  [self updateAccessibilityAttributes];
  self.needsDisplay = YES;
}

- (void)setSharingText:(NSString*)sharingText {
  if (_sharingText == sharingText ||
      (sharingText && [_sharingText isEqualToString:sharingText])) {
    return;
  }
  _sharingText = [sharingText copy];
  [self updateAccessibilityAttributes];
  self.needsDisplay = YES;
}

- (void)setIconImage:(NSImage*)iconImage {
  if (_iconImage == iconImage ||
      (iconImage && [_iconImage isEqual:iconImage])) {
    return;
  }
  _iconImage = iconImage;
  self.needsDisplay = YES;
}

- (void)setIsRTL:(BOOL)isRTL {
  const BOOL normalizedIsRTL = isRTL ? YES : NO;
  if (_isRTL == normalizedIsRTL) {
    return;
  }
  _isRTL = normalizedIsRTL;
  self.userInterfaceLayoutDirection =
      _isRTL ? NSUserInterfaceLayoutDirectionRightToLeft
             : NSUserInterfaceLayoutDirectionLeftToRight;
  self.needsDisplay = YES;
}

- (void)setTextScale:(CGFloat)textScale {
  const CGFloat clampedScale =
      std::clamp(textScale, kMinTextScaleFactor, kMaxTextScaleFactor);
  if (std::abs(_textScale - clampedScale) <= 1e-6) {
    return;
  }
  _textScale = clampedScale;
  self.needsDisplay = YES;
}

- (void)setHighContrastOverrideForTesting:(std::optional<bool>)highContrast {
  if (_highContrastOverrideForTesting == highContrast) {
    return;
  }
  _highContrastOverrideForTesting = highContrast;
  self.needsDisplay = YES;
}

- (void)updateAccessibilityAttributes {
  NSString* accessibleText =
      _sharingText.length > 0 ? _sharingText : (_titleText ?: @"");
  self.accessibilityValue = accessibleText;
}

- (BOOL)shouldUseHighContrastColors {
  if (_highContrastOverrideForTesting.has_value()) {
    return *_highContrastOverrideForTesting;
  }
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast;
}

- (remoting::BackdropContentLayout)contentLayoutForBounds:(NSRect)bounds {
  return ComputeBackdropRenderLayout(bounds, _titleText, _sharingText,
                                     NSColor.whiteColor, _isRTL, _textScale)
      .rects;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect bounds = self.bounds;
  if (NSWidth(bounds) <= 0.0 || NSHeight(bounds) <= 0.0) {
    return;
  }

  const BOOL highContrast = [self shouldUseHighContrastColors];
  NSColor* backgroundColor = highContrast
                                 ? NSColor.windowBackgroundColor
                                 : [NSColor colorWithSRGBRed:kGoogleBlueRed
                                                       green:kGoogleBlueGreen
                                                        blue:kGoogleBlueBlue
                                                       alpha:1.0];
  NSColor* textColor = highContrast ? NSColor.textColor : NSColor.whiteColor;

  [backgroundColor setFill];
  NSRectFill(dirtyRect);

  BackdropRenderLayout layout = ComputeBackdropRenderLayout(
      bounds, _titleText, _sharingText, textColor, _isRTL, _textScale);

  if (_iconImage) {
    [_iconImage drawInRect:layout.rects.icon_rect
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0
            respectFlipped:YES
                     hints:nil];
  }

  if (_titleText.length > 0 && NSHeight(layout.rects.title_rect) > 0.0) {
    [_titleText drawWithRect:layout.rects.title_rect
                     options:NSStringDrawingUsesLineFragmentOrigin |
                             NSStringDrawingUsesFontLeading
                  attributes:layout.title_attrs];
  }

  if (_sharingText.length > 0 && NSHeight(layout.rects.sharing_rect) > 0.0) {
    [_sharingText drawWithRect:layout.rects.sharing_rect
                       options:NSStringDrawingUsesLineFragmentOrigin |
                               NSStringDrawingUsesFontLeading
                    attributes:layout.body_attrs];
  }
}

@end

@implementation PrivacyOverlayBackdropWindow

- (instancetype)initWithScreenFrame:(NSRect)screenFrame {
  if (NSWidth(screenFrame) <= 0.0 || NSHeight(screenFrame) <= 0.0) {
    return nil;
  }
  self = [super initWithContentRect:screenFrame
                          styleMask:NSWindowStyleMaskBorderless
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    self.releasedWhenClosed = NO;
    self.canHide = NO;
    self.movable = NO;
    self.opaque = YES;
    self.hasShadow = NO;
    self.ignoresMouseEvents = YES;
    self.level = NSScreenSaverWindowLevel;
    self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                              NSWindowCollectionBehaviorFullScreenAuxiliary |
                              NSWindowCollectionBehaviorStationary |
                              NSWindowCollectionBehaviorIgnoresCycle;
    self.sharingType = NSWindowSharingNone;
    [self setFrame:screenFrame display:NO];
    self.contentView = [[PrivacyOverlayBackdropView alloc]
        initWithFrame:NSMakeRect(0, 0, NSWidth(screenFrame),
                                 NSHeight(screenFrame))];
    if (!ApplyCaptureExclusion(self)) {
      [self orderOut:nil];
      [self close];
      return nil;
    }
  }
  return self;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  return frameRect;
}

- (BOOL)canBecomeKeyWindow {
  return NO;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}

@end

@implementation PrivacyOverlayButtonView {
  std::optional<bool> _highContrastOverrideForTesting;
}

- (void)setHighContrastOverrideForTesting:(std::optional<bool>)highContrast {
  if (_highContrastOverrideForTesting == highContrast) {
    return;
  }
  _highContrastOverrideForTesting = highContrast;
  self.needsDisplay = YES;
}

- (BOOL)shouldUseHighContrastColors {
  if (_highContrastOverrideForTesting.has_value()) {
    return *_highContrastOverrideForTesting;
  }
  return NSWorkspace.sharedWorkspace.accessibilityDisplayShouldIncreaseContrast;
}

- (void)drawRect:(NSRect)dirtyRect {
  NSRect insetBounds = NSInsetRect(self.bounds, kButtonBorderWidth / 2.0,
                                   kButtonBorderWidth / 2.0);
  if (NSWidth(insetBounds) <= 0.0 || NSHeight(insetBounds) <= 0.0) {
    return;
  }

  const BOOL highContrast = [self shouldUseHighContrastColors];
  NSColor* backgroundColor = highContrast
                                 ? NSColor.windowBackgroundColor
                                 : [NSColor colorWithSRGBRed:kGoogleBlueRed
                                                       green:kGoogleBlueGreen
                                                        blue:kGoogleBlueBlue
                                                       alpha:1.0];
  NSColor* borderColor = highContrast ? NSColor.textColor : NSColor.whiteColor;

  NSBezierPath* path =
      [NSBezierPath bezierPathWithRoundedRect:insetBounds
                                      xRadius:kButtonCornerRadius
                                      yRadius:kButtonCornerRadius];
  [backgroundColor setFill];
  [path fill];
  path.lineWidth = kButtonBorderWidth;
  [borderColor setStroke];
  [path stroke];
}

@end

@implementation PrivacyOverlayButtonWindow {
  base::RepeatingClosure _onDisconnect;
  CGFloat _appliedTextScale;
  NSSize _naturalButtonSize;
  NSString* __strong _measuredButtonTitle;
}

@synthesize disconnectButton = _disconnectButton;
@synthesize buttonView = _buttonView;

- (instancetype)initWithTitle:(NSString*)buttonTitle
                  windowTitle:(NSString*)windowTitle
                        isRTL:(BOOL)isRTL
                    textScale:(CGFloat)textScale
                 onDisconnect:(base::RepeatingClosure)onDisconnect {
  NSRect initialFrame = NSMakeRect(0, 0, kMinButtonWidth + kButtonMargin * 2.0,
                                   kMinButtonHeight + kButtonMargin * 2.0);
  self = [super initWithContentRect:initialFrame
                          styleMask:NSWindowStyleMaskBorderless |
                                    NSWindowStyleMaskNonactivatingPanel
                            backing:NSBackingStoreBuffered
                              defer:NO];
  if (self) {
    _onDisconnect = std::move(onDisconnect);
    _appliedTextScale = 0.0;
    _naturalButtonSize = NSZeroSize;
    self.releasedWhenClosed = NO;
    self.hidesOnDeactivate = NO;
    self.canHide = NO;
    self.movable = NO;
    self.backgroundColor = NSColor.clearColor;
    self.opaque = NO;
    self.hasShadow = NO;
    self.level = NSScreenSaverWindowLevel + 1;
    self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces |
                              NSWindowCollectionBehaviorFullScreenAuxiliary |
                              NSWindowCollectionBehaviorStationary |
                              NSWindowCollectionBehaviorIgnoresCycle;
    self.sharingType = NSWindowSharingNone;
    self.title = windowTitle ?: @"";

    _buttonView = [[PrivacyOverlayButtonView alloc] initWithFrame:initialFrame];
    _buttonView.userInterfaceLayoutDirection =
        isRTL ? NSUserInterfaceLayoutDirectionRightToLeft
              : NSUserInterfaceLayoutDirectionLeftToRight;
    self.contentView = _buttonView;

    _disconnectButton = [[NSButton alloc]
        initWithFrame:NSMakeRect(kButtonMargin, kButtonMargin, kMinButtonWidth,
                                 kMinButtonHeight)];
    _disconnectButton.buttonType = NSButtonTypeMomentaryPushIn;
    _disconnectButton.bezelStyle = NSBezelStyleFlexiblePush;
    _disconnectButton.lineBreakMode = NSLineBreakByTruncatingTail;
    _disconnectButton.title = buttonTitle ?: @"";
    _disconnectButton.accessibilityLabel = buttonTitle ?: @"";
    _disconnectButton.userInterfaceLayoutDirection =
        isRTL ? NSUserInterfaceLayoutDirectionRightToLeft
              : NSUserInterfaceLayoutDirectionLeftToRight;
    _disconnectButton.target = self;
    _disconnectButton.action = @selector(onDisconnectClicked:);
    [_buttonView addSubview:_disconnectButton];

    NSSize initialSize =
        [self updateFontAndComputeWindowSizeForTextScale:textScale
                                          maxWindowWidth:0.0];
    if (![self applyWindowFrame:NSMakeRect(0, 0, initialSize.width,
                                           initialSize.height)
                   applyCaptureExclusion:NO
            forceCaptureExclusionRefresh:NO]) {
      [self orderOut:nil];
      [self close];
      return nil;
    }
    // Capture exclusion on `button_window_` is best-effort (`sharingType =
    // NSWindowSharingNone` is still set, and remote clicks are dropped in
    // `-sendEvent:`): unlike `backdrop_windows_`, failing to apply the CGS
    // capture-exclude shape on the Disconnect button does not expose local
    // screen contents or block the remote desktop, so keep the session active.
    if (!ApplyCaptureExclusion(self)) {
      LOG(WARNING)
          << "Failed to apply initial capture exclusion to Disconnect button";
    }
  }
  return self;
}

- (NSSize)updateFontAndComputeWindowSizeForTextScale:(CGFloat)textScale
                                      maxWindowWidth:(CGFloat)maxWindowWidth {
  if (!_disconnectButton || !_buttonView) {
    return NSZeroSize;
  }

  const CGFloat clampedScale =
      std::clamp(textScale, kMinTextScaleFactor, kMaxTextScaleFactor);
  NSString* currentTitle = _disconnectButton.title ?: @"";
  if (_naturalButtonSize.width <= 0.0 || _naturalButtonSize.height <= 0.0 ||
      std::abs(_appliedTextScale - clampedScale) > 1e-6 ||
      !_measuredButtonTitle ||
      ![_measuredButtonTitle isEqualToString:currentTitle]) {
    _disconnectButton.font =
        [NSFont systemFontOfSize:ScaleFontSize(kBodyFontSize, clampedScale)];
    [_disconnectButton sizeToFit];
    _naturalButtonSize = _disconnectButton.frame.size;
    _appliedTextScale = clampedScale;
    _measuredButtonTitle = [currentTitle copy];
  }

  CGFloat minButtonWidth = std::ceil(kMinButtonWidth * clampedScale);
  CGFloat minButtonHeight = std::ceil(kMinButtonHeight * clampedScale);
  CGFloat buttonWidth =
      std::max(minButtonWidth, std::ceil(_naturalButtonSize.width));
  CGFloat buttonHeight =
      std::max(minButtonHeight, std::ceil(_naturalButtonSize.height));
  CGFloat windowWidth = buttonWidth + kButtonMargin * 2.0;
  if (maxWindowWidth > 0.0 && windowWidth > maxWindowWidth) {
    windowWidth = maxWindowWidth;
  }
  return NSMakeSize(windowWidth, buttonHeight + kButtonMargin * 2.0);
}

- (BOOL)applyWindowFrame:(NSRect)targetFrame
           applyCaptureExclusion:(BOOL)applyCaptureExclusion
    forceCaptureExclusionRefresh:(BOOL)forceCaptureExclusionRefresh {
  if (!_disconnectButton || !_buttonView || NSWidth(targetFrame) <= 0.0 ||
      NSHeight(targetFrame) <= 0.0) {
    return NO;
  }

  NSRect oldFrame = self.frame;
  const BOOL sizeChanged = !NSEqualSizes(oldFrame.size, targetFrame.size);

  // Expand the capture-exclusion region to the union size before growing a live
  // window frame so enlarged corners cannot flash to WindowServer capture, then
  // tighten to `targetFrame.size` afterward (or refresh unconditionally when
  // `forceCaptureExclusionRefresh` is YES). Capture exclusion on
  // `button_window_` is best-effort (`sharingType = NSWindowSharingNone` is
  // still set, and remote clicks are dropped in `-sendEvent:`), so log a
  // warning on failure and still apply `targetFrame`.
  if (applyCaptureExclusion && sizeChanged) {
    NSSize unionSize =
        NSMakeSize(std::max(NSWidth(oldFrame), NSWidth(targetFrame)),
                   std::max(NSHeight(oldFrame), NSHeight(targetFrame)));
    if (!NSEqualSizes(oldFrame.size, unionSize) &&
        !ApplyCaptureExclusionForSize(self, unionSize)) {
      LOG(WARNING)
          << "Failed to expand capture exclusion before resizing Disconnect "
          << "button";
    }
  }

  const BOOL frameChanged = !NSEqualRects(oldFrame, targetFrame);
  if (frameChanged) {
    [self setFrame:targetFrame display:NO];
  }

  if (applyCaptureExclusion && (sizeChanged || forceCaptureExclusionRefresh) &&
      !ApplyCaptureExclusion(self)) {
    LOG(WARNING) << "Failed to update capture exclusion for Disconnect button";
  }

  NSSize appliedSize = self.frame.size;
  CGFloat buttonWidth =
      std::max<CGFloat>(1.0, appliedSize.width - kButtonMargin * 2.0);
  CGFloat buttonHeight =
      std::max<CGFloat>(1.0, appliedSize.height - kButtonMargin * 2.0);
  _buttonView.frame = NSMakeRect(0, 0, appliedSize.width, appliedSize.height);
  if (self.contentView != _buttonView) {
    self.contentView.frame =
        NSMakeRect(0, 0, appliedSize.width, appliedSize.height);
  }
  _disconnectButton.frame =
      NSMakeRect(kButtonMargin, kButtonMargin, buttonWidth, buttonHeight);
  if (frameChanged && !self.ignoresMouseEvents && self.alphaValue > 0.0) {
    [self displayIfNeeded];
  }
  return YES;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen*)screen {
  return frameRect;
}

- (BOOL)canBecomeKeyWindow {
  return NO;
}

- (BOOL)canBecomeMainWindow {
  return NO;
}

- (void)sendEvent:(NSEvent*)event {
  // Defense-in-depth: ignore mouse/key events synthesized by the CRD host
  // process (`InputInjectorMac`) if local and remote inputs race while the
  // Disconnect button window is visible.
  if (IsRemoteInjectedEvent(event)) {
    return;
  }
  [super sendEvent:event];
}

- (void)onDisconnectClicked:(id)sender {
  // Remote-injected mouse and key `NSEvent`s are filtered upstream in
  // `-sendEvent:`. Do not inspect `NSApp.currentEvent` here: AppKit retains
  // the last dequeued `NSEvent` across runloop turns, which would falsely
  // block local VoiceOver (`AXPress`) / `performClick:` activations if the most
  // recent app event happened to be remote-injected.
  if (base::RepeatingClosure on_disconnect = std::exchange(_onDisconnect, {})) {
    // Defer teardown to the next sequence turn so `PrivacyOverlayButtonWindow`
    // and `_disconnectButton` remain alive (and enabled while
    // `accessibilityPerformPress` / `performClick:` unwinds) until AppKit
    // finishes dispatching the button action.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_disconnect));
  }
}

@end

namespace remoting {

PrivacyOverlayMac::PrivacyOverlayMac(
    std::unique_ptr<LocalInputMonitor> local_input_monitor)
    : local_input_monitor_(std::move(local_input_monitor)),
      backdrop_windows_([[NSMutableArray alloc] init]) {}

PrivacyOverlayMac::~PrivacyOverlayMac() {
  EndOverlay();
}

void PrivacyOverlayMac::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_session_control);

  DisconnectWindowBase::Start(client_session_control);

  if (!BeginOverlay()) {
    EndOverlay();
    DisconnectSession("Failed to initialize Privacy Overlay.");
    return;
  }

  if (local_input_monitor_) {
    // Both `LocalMouseInputMonitorMac` (which forwards move, click, drag, and
    // scroll `ui::EventType`s) and `LocalKeyboardInputMonitorMac` (which taps
    // `kCGEventKeyDown` / `kCGEventKeyUp`) filter out events synthesized by the
    // CRD host process (`kCGEventSourceUnixProcessID == getpid()`), so remote
    // input will not reveal or hold open the Disconnect button.
    // TODO(crbug.com/451238243): Have `LocalMouseInputMonitorMac` also invoke
    // `disconnect_callback` on tap creation failure or mid-session
    // `kCGEventTapDisabledByUserInput` revocation; currently the `on_error`
    // fail-open path relies on `LocalKeyboardInputMonitorMac`.
    local_input_monitor_->StartMonitoring(
        base::BindRepeating(&PrivacyOverlayMac::OnLocalMouseEvent,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PrivacyOverlayMac::OnLocalKeyPressed,
                            weak_factory_.GetWeakPtr()),
        base::BindRepeating(&PrivacyOverlayMac::StopAutoHideBehavior,
                            weak_factory_.GetWeakPtr()));
  } else {
    StopAutoHideBehavior();
  }
}

// static
base::AutoReset<PrivacyOverlayMac::ScreenFramesProvider>
PrivacyOverlayMac::SetScreenFramesProviderForTesting(
    ScreenFramesProvider provider) {
  return base::AutoReset<ScreenFramesProvider>(
      &GetScreenFramesProviderStorage(), std::move(provider));
}

// static
base::AutoReset<PrivacyOverlayMac::CaptureExclusionApplier>
PrivacyOverlayMac::SetCaptureExclusionApplierForTesting(
    CaptureExclusionApplier applier) {
  return base::AutoReset<CaptureExclusionApplier>(
      &GetCaptureExclusionApplierStorage(), std::move(applier));
}

// static
base::AutoReset<PrivacyOverlayMac::MouseLocationProvider>
PrivacyOverlayMac::SetMouseLocationProviderForTesting(
    MouseLocationProvider provider) {
  return base::AutoReset<MouseLocationProvider>(
      &GetMouseLocationProviderStorage(), std::move(provider));
}

// static
base::ScopedClosureRunner
PrivacyOverlayMac::SetUniversalAccessDefaultsForTesting(
    NSUserDefaults* defaults,
    NSString* bundle_id) {
  auto& storage = GetUniversalAccessTestingOverrideStorage();
  const bool prev_active = storage.active;
  NSUserDefaults* prev_defaults = storage.defaults;
  NSString* prev_bundle_id = storage.bundle_id;
  storage.active = true;
  storage.defaults = defaults;
  storage.bundle_id = [bundle_id copy];
  return base::ScopedClosureRunner(base::BindOnce(
      [](bool prev_active, NSUserDefaults* prev_defaults,
         NSString* prev_bundle_id) {
        auto& storage = GetUniversalAccessTestingOverrideStorage();
        storage.active = prev_active;
        storage.defaults = prev_defaults;
        storage.bundle_id = prev_bundle_id;
      },
      prev_active, prev_defaults, prev_bundle_id));
}

// static
CGFloat PrivacyOverlayMac::TextScaleFactorForCategoryNameForTesting(
    NSString* category) {
  return TextScaleFactorForCategory(category);
}

// static
bool PrivacyOverlayMac::IsRemoteInjectedEventForTesting(NSEvent* event) {
  return IsRemoteInjectedEvent(event);
}

bool PrivacyOverlayMac::BeginOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!NSApp) {
    [NSApplication sharedApplication];
  }

  product_name_text_ = l10n_util::GetNSString(IDS_PRODUCT_NAME);
  if (product_name_text_.length == 0) {
    LOG(ERROR) << "Failed to load IDS_PRODUCT_NAME for Privacy Overlay";
    return false;
  }

  button_text_ = l10n_util::GetNSString(IDS_STOP_SHARING_BUTTON);
  if (button_text_.length == 0) {
    LOG(ERROR) << "Failed to load IDS_STOP_SHARING_BUTTON for Privacy Overlay";
    return false;
  }

  std::u16string message_template =
      l10n_util::GetStringUTF16(IDS_MESSAGE_SHARED);
  is_rtl_ = base::i18n::IsRTL() ||
            base::i18n::StringContainsStrongRTLChars(message_template);

  // `formatted_email()` already applies macOS BiDi wrapping via
  // `AdjustStringForLocaleDirection()` when `base::i18n::IsRTL()` is true. Only
  // wrap with explicit LTR marks when the ICU UI locale is LTR while
  // `IDS_MESSAGE_SHARED` itself contains strong RTL characters.
  std::u16string email_for_display = formatted_email();
  if (is_rtl_ && !base::i18n::IsRTL() &&
      !base::i18n::StringContainsStrongRTLChars(email_for_display)) {
    base::i18n::WrapStringWithLTRFormatting(&email_for_display);
  }

  sharing_with_text_ =
      l10n_util::GetNSStringF(IDS_MESSAGE_SHARED, email_for_display);
  if (sharing_with_text_.length == 0) {
    LOG(ERROR) << "Failed to load IDS_MESSAGE_SHARED for Privacy Overlay";
    return false;
  }

  icon_image_ = LoadOverlayIcon();
  cached_text_scale_ =
      GetSystemTextScaleFactor(CreateUniversalAccessDefaults());

  if (!UpdateBackdrops()) {
    return false;
  }

  button_window_ = [[PrivacyOverlayButtonWindow alloc]
      initWithTitle:button_text_
        windowTitle:product_name_text_
              isRTL:is_rtl_
          textScale:cached_text_scale_
       onDisconnect:base::BindRepeating(
                        &PrivacyOverlayMac::OnDisconnectButtonClicked,
                        weak_factory_.GetWeakPtr())];
  if (!button_window_) {
    return false;
  }

  const std::vector<NSRect> screen_frames = GetScreenFrames();
  NSPoint initial_cocoa_pos = GetCurrentMouseLocation();
  mouse_position_ = ConvertCocoaPointToQuartz(screen_frames, initial_cocoa_pos);

  PositionDisconnectButton(screen_frames, initial_cocoa_pos,
                           /*force_capture_exclusion_refresh=*/false);
  DeactivateDisconnectButton();

  notification_observer_ = [[PrivacyOverlayNotificationObserver alloc]
      initWithOverlay:weak_factory_.GetWeakPtr()
         buttonWindow:button_window_];

  NSAccessibilityPostNotificationWithUserInfo(
      NSApp, NSAccessibilityAnnouncementRequestedNotification, @{
        NSAccessibilityAnnouncementKey : sharing_with_text_,
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
      });

  return true;
}

void PrivacyOverlayMac::EndOverlay() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (is_ending_) {
    return;
  }
  is_ending_ = true;

  auto_hide_timer_.Stop();
  local_input_monitor_.reset();
  active_button_screen_frame_.reset();
  cached_text_scale_ = kMinTextScaleFactor;

  if (notification_observer_) {
    [notification_observer_ invalidate];
    notification_observer_ = nil;
  }

  for (PrivacyOverlayBackdropWindow* window in backdrop_windows_) {
    [window orderOut:nil];
    [window close];
  }
  [backdrop_windows_ removeAllObjects];

  if (button_window_) {
    is_button_hidden_ = true;
    PrivacyOverlayButtonWindow* button_window =
        std::exchange(button_window_, nil);
    [button_window orderOut:nil];
    [button_window close];
  }
}

bool PrivacyOverlayMac::UpdateBackdrops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Invalidate the cached button display frame so `ActivateDisconnectButton` /
  // `PositionDisconnectButton` recomputes coordinates for updated geometry.
  active_button_screen_frame_.reset();

  const std::vector<NSRect> screen_frames = GetScreenFrames();
  if (screen_frames.empty()) {
    LOG(ERROR) << "No active displays found for Privacy Overlay";
    return false;
  }

  while (backdrop_windows_.count > screen_frames.size()) {
    PrivacyOverlayBackdropWindow* excess_window = backdrop_windows_.lastObject;
    [excess_window orderOut:nil];
    [excess_window close];
    [backdrop_windows_ removeLastObject];
  }

  for (size_t i = 0; i < screen_frames.size(); ++i) {
    const NSRect& screen_frame = screen_frames[i];
    PrivacyOverlayBackdropWindow* window = nil;
    if (i < backdrop_windows_.count) {
      window = backdrop_windows_[i];
      NSRect old_frame = window.frame;
      if (!NSEqualRects(old_frame, screen_frame)) {
        NSSize union_size =
            NSMakeSize(std::max(NSWidth(old_frame), NSWidth(screen_frame)),
                       std::max(NSHeight(old_frame), NSHeight(screen_frame)));
        if (!NSEqualSizes(old_frame.size, union_size) &&
            !ApplyCaptureExclusionForSize(window, union_size)) {
          return false;
        }
        [window setFrame:screen_frame display:NO];
        if (!NSEqualSizes(old_frame.size, window.frame.size) &&
            !ApplyCaptureExclusion(window)) {
          return false;
        }
      }
    } else {
      window = [[PrivacyOverlayBackdropWindow alloc]
          initWithScreenFrame:screen_frame];
      if (!window) {
        return false;
      }
      [backdrop_windows_ addObject:window];
    }

    window.title = product_name_text_;
    PrivacyOverlayBackdropView* view =
        base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
            window.contentView);
    NSRect view_frame =
        NSMakeRect(0, 0, NSWidth(screen_frame), NSHeight(screen_frame));
    if (!NSEqualRects(view.frame, view_frame)) {
      view.frame = view_frame;
    }
    view.titleText = product_name_text_;
    view.sharingText = sharing_with_text_;
    view.iconImage = icon_image_;
    view.isRTL = is_rtl_;
    view.textScale = cached_text_scale_;

    [window orderFrontRegardless];
  }

  return true;
}

void PrivacyOverlayMac::PositionDisconnectButton(
    const std::vector<NSRect>& screen_frames,
    NSPoint cocoa_screen_point,
    bool force_capture_exclusion_refresh) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!button_window_) {
    return;
  }

  DCHECK(!screen_frames.empty());
  if (screen_frames.empty()) {
    return;
  }

  size_t screen_index =
      FindScreenIndexForCocoaPoint(screen_frames, cocoa_screen_point);
  const NSRect& screen_frame = screen_frames[screen_index];
  CGFloat screen_width = NSWidth(screen_frame);
  CGFloat screen_height = NSHeight(screen_frame);
  CGFloat max_window_width =
      (screen_width > kButtonScreenHorizontalMargin * 2.0)
          ? (screen_width - kButtonScreenHorizontalMargin * 2.0)
          : std::max<CGFloat>(1.0, screen_width);
  NSSize button_size = [button_window_
      updateFontAndComputeWindowSizeForTextScale:cached_text_scale_
                                  maxWindowWidth:max_window_width];
  if (button_size.width <= 0.0 || button_size.height <= 0.0) {
    LOG(ERROR) << "Invalid Disconnect button window size: " << button_size.width
               << "x" << button_size.height;
    return;
  }
  CGFloat button_width = button_size.width;
  CGFloat button_height = button_size.height;

  // Measure the wrapped sharing text bounds directly from the current text,
  // RTL state, and `cached_text_scale_` (independent of `backdrop_windows_`) so
  // the floating Disconnect button is placed below multi-line text whenever
  // vertical screen space permits. Clamp to `kVerticalSpacing` from the bottom
  // edge so the button always remains fully on-screen (overlapping text only
  // when a display is too short to fit both).
  NSRect sharing_rect =
      ComputeBackdropRenderLayout(
          NSMakeRect(0, 0, screen_width, screen_height), product_name_text_,
          sharing_with_text_, NSColor.whiteColor, is_rtl_, cached_text_scale_)
          .rects.sharing_rect;
  CGFloat min_flipped_button_top =
      std::max<CGFloat>((screen_height / 2.0) + kButtonTopOffset,
                        NSMaxY(sharing_rect) + kVerticalSpacing);
  CGFloat max_flipped_button_top = std::max<CGFloat>(
      kVerticalSpacing, screen_height - button_height - kVerticalSpacing);
  CGFloat flipped_button_top =
      std::min(min_flipped_button_top, max_flipped_button_top);

  CGFloat cocoa_x =
      std::round(NSMinX(screen_frame) + (screen_width - button_width) / 2.0);
  CGFloat cocoa_y =
      std::round(NSMaxY(screen_frame) - flipped_button_top - button_height);

  active_button_screen_frame_ = screen_frame;

  // Store expected origin in global Cocoa screen coordinates (matching
  // `button_window_.frame.origin`, bottom-left origin of primary display)
  // before `setFrame:` (which synchronously posts `NSWindowDidMoveNotification`
  // to `OnButtonWindowDidMove()`), then apply both size and centered origin in
  // a single `setFrame:` call. `button_size` and subviews were already
  // validated by `updateFontAndComputeWindowSizeForTextScale:maxWindowWidth:`
  // above.
  SetExpectedPosition(static_cast<int>(cocoa_x), static_cast<int>(cocoa_y));
  const BOOL applied = [button_window_
                  applyWindowFrame:NSMakeRect(cocoa_x, cocoa_y, button_width,
                                              button_height)
             applyCaptureExclusion:YES
      forceCaptureExclusionRefresh:force_capture_exclusion_refresh ? YES : NO];
  DCHECK(applied);
}

void PrivacyOverlayMac::ActivateDisconnectButton(NSPoint cocoa_screen_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActivateDisconnectButtonWithFrames(GetScreenFrames(), cocoa_screen_point);
}

void PrivacyOverlayMac::ActivateDisconnectButtonForQuartzPoint(
    const webrtc::DesktopVector& quartz_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::vector<NSRect> screen_frames = GetScreenFrames();
  ActivateDisconnectButtonWithFrames(
      screen_frames, ConvertQuartzPointToCocoa(screen_frames, quartz_point));
}

void PrivacyOverlayMac::ActivateDisconnectButtonWithFrames(
    const std::vector<NSRect>& screen_frames,
    NSPoint cocoa_screen_point) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!button_window_) {
    return;
  }

  if (auto_hide_enabled_) {
    auto_hide_timer_.Start(
        FROM_HERE, kAutoHideTimeout,
        base::BindOnce(&PrivacyOverlayMac::DeactivateDisconnectButton,
                       weak_factory_.GetWeakPtr()));
  }

  if (screen_frames.empty()) {
    return;
  }

  if (!is_button_hidden_ && active_button_screen_frame_.has_value()) {
    const NSRect& target_frame = screen_frames[FindScreenIndexForCocoaPoint(
        screen_frames, cocoa_screen_point)];
    if (NSEqualRects(*active_button_screen_frame_, target_frame)) {
      return;
    }
  }

  PositionDisconnectButton(screen_frames, cocoa_screen_point,
                           /*force_capture_exclusion_refresh=*/true);
  is_button_hidden_ = false;
  button_window_.ignoresMouseEvents = NO;
  button_window_.alphaValue = 1.0;
  [button_window_ orderFrontRegardless];
  NSAccessibilityPostNotification(button_window_.disconnectButton,
                                  NSAccessibilityLayoutChangedNotification);
}

void PrivacyOverlayMac::DeactivateDisconnectButton() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!button_window_) {
    return;
  }

  is_button_hidden_ = true;
  button_window_.ignoresMouseEvents = YES;
  button_window_.alphaValue = 0.0;
  [button_window_ orderOut:nil];
}

void PrivacyOverlayMac::StopAutoHideBehavior() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto_hide_enabled_ = false;
  auto_hide_timer_.Stop();
  if (local_input_monitor_) {
    // Release asynchronously in case a `LocalInputMonitor` implementation
    // invokes `on_error` synchronously before unwinding its own stack frame.
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(local_input_monitor_));
  }
  ActivateDisconnectButtonForQuartzPoint(mouse_position_);
}

void PrivacyOverlayMac::OnLocalMouseEvent(const webrtc::DesktopVector& position,
                                          ui::EventType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (std::abs(position.x() - mouse_position_.x()) > kMouseJitterThresholdPt ||
      std::abs(position.y() - mouse_position_.y()) > kMouseJitterThresholdPt ||
      type != ui::EventType::kMouseMoved) {
    mouse_position_ = position;
    ActivateDisconnectButtonForQuartzPoint(position);
  }
}

void PrivacyOverlayMac::OnLocalKeyPressed(uint32_t /*usb_keycode*/) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActivateDisconnectButtonForQuartzPoint(mouse_position_);
}

void PrivacyOverlayMac::OnScreenParametersChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_ending_) {
    return;
  }

  ResetRepositionAttempts();
  if (!UpdateBackdrops()) {
    EndOverlay();
    DisconnectSession("Failed to update Privacy Overlay backdrops.");
    return;
  }
  const std::vector<NSRect> screen_frames = GetScreenFrames();
  PositionDisconnectButton(
      screen_frames, ConvertQuartzPointToCocoa(screen_frames, mouse_position_),
      /*force_capture_exclusion_refresh=*/!is_button_hidden_);
  if (!is_button_hidden_ && button_window_) {
    [button_window_ orderFrontRegardless];
  }
}

void PrivacyOverlayMac::OnButtonWindowDidMove() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_ending_ || is_repositioning_button_ || !button_window_ ||
      !expected_x().has_value() || !expected_y().has_value()) {
    return;
  }

  // During display reconfiguration, AppKit may relocate `button_window_` and
  // post `NSWindowDidMoveNotification` before
  // `NSApplicationDidChangeScreenParametersNotification`. Skip snap-back if the
  // expected origin no longer lies within any current display frame; the
  // subsequent `OnScreenParametersChanged()` callback will recompute the button
  // position and reset reposition attempts for the new geometry. Note that if
  // AppKit posts `NSWindowDidMoveNotification` before `NSScreen.screens` itself
  // reflects the new configuration, `GetScreenFrames()` still returns stale
  // geometry and a single transient snap-back can occur before
  // `OnScreenParametersChanged()` corrects it.
  const NSPoint expected_origin = NSMakePoint(*expected_x(), *expected_y());
  const std::vector<NSRect> screen_frames = GetScreenFrames();
  const bool expected_origin_on_screen = std::ranges::any_of(
      screen_frames, [&expected_origin](const NSRect& screen_frame) {
        // Use `NSPointInRect` (inclusive of `NSMinX` / `NSMinY`, exclusive of
        // `NSMaxX` / `NSMaxY`) rather than `NSMouseInRect(..., /*flipped=*/NO)`
        // because `expected_origin` is a bottom-left window frame origin rather
        // than a cursor coordinate.
        return NSPointInRect(expected_origin, screen_frame);
      });
  if (!expected_origin_on_screen) {
    return;
  }

  NSRect frame = button_window_.frame;
  int actual_x = static_cast<int>(std::lround(frame.origin.x));
  int actual_y = static_cast<int>(std::lround(frame.origin.y));
  if (ShouldRepositionOnDisplacement(actual_x, actual_y)) {
    base::AutoReset<bool> repositioning(&is_repositioning_button_, true);
    [button_window_ setFrameOrigin:expected_origin];
  }
}

void PrivacyOverlayMac::OnAccessibilityDisplayOptionsChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (is_ending_) {
    return;
  }

  for (PrivacyOverlayBackdropWindow* window in backdrop_windows_) {
    window.contentView.needsDisplay = YES;
  }
  if (button_window_) {
    button_window_.contentView.needsDisplay = YES;
  }
}

void PrivacyOverlayMac::OnDisconnectButtonClicked() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndOverlay();
  DisconnectSession();
}

// static
std::unique_ptr<HostWindow> HostWindow::CreatePrivacyOverlay(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  return std::make_unique<PrivacyOverlayMac>(std::move(local_input_monitor));
}

}  // namespace remoting
