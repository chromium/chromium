// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/privacy_overlay_mac.h"

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/no_destructor.h"
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

// Layout constants (in Cocoa points) for the privacy overlay backdrop content.
constexpr CGFloat kIconSize = 64.0;
constexpr CGFloat kTitleFontSize = 21.0;
constexpr CGFloat kBodyFontSize = 14.0;
constexpr CGFloat kVerticalSpacing = 16.0;
constexpr CGFloat kMaxTextWidthRatio = 0.8;

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

struct BackdropContentLayout {
  NSDictionary<NSAttributedStringKey, id>* __strong title_attrs;
  NSDictionary<NSAttributedStringKey, id>* __strong body_attrs;
  NSRect icon_rect = NSZeroRect;
  NSRect title_rect = NSZeroRect;
  NSRect sharing_rect = NSZeroRect;
};

BackdropContentLayout ComputeBackdropContentLayout(NSRect bounds,
                                                   NSString* title_text,
                                                   NSString* sharing_text,
                                                   NSColor* text_color,
                                                   BOOL is_rtl) {
  BackdropContentLayout layout;
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

  NSFont* title_font = [NSFont systemFontOfSize:kTitleFontSize
                                         weight:NSFontWeightSemibold];
  NSFont* body_font = [NSFont systemFontOfSize:kBodyFontSize
                                        weight:NSFontWeightRegular];

  layout.title_attrs = CreateTextAttributes(title_font, text_color, is_rtl);
  layout.body_attrs = CreateTextAttributes(body_font, text_color, is_rtl);

  CGFloat title_height = MeasureWrappedTextHeight(
      title_text, max_text_width, layout.title_attrs, kTitleFontSize);
  CGFloat body_height =
      MeasureWrappedTextHeight(sharing_text, max_text_width, layout.body_attrs,
                               kBodyFontSize + kVerticalSpacing);

  const CGFloat min_icon_top = NSMinY(bounds) + kVerticalSpacing;
  CGFloat title_bottom = center_y - kVerticalSpacing;
  CGFloat icon_top = title_bottom - title_height - kVerticalSpacing - kIconSize;
  if (icon_top < min_icon_top) {
    CGFloat shift_down = min_icon_top - icon_top;
    icon_top = min_icon_top;
    title_bottom += shift_down;
    center_y += shift_down;
  }

  layout.icon_rect = NSMakeRect(std::round(center_x - kIconSize / 2.0),
                                std::round(icon_top), kIconSize, kIconSize);
  layout.title_rect =
      NSMakeRect(text_left, std::round(title_bottom - title_height),
                 max_text_width, title_height);
  layout.sharing_rect =
      NSMakeRect(text_left, std::round(center_y), max_text_width, body_height);
  return layout;
}

}  // namespace

@implementation PrivacyOverlayBackdropView {
  std::optional<bool> _highContrastOverrideForTesting;
}

@synthesize titleText = _titleText;
@synthesize sharingText = _sharingText;
@synthesize iconImage = _iconImage;
@synthesize isRTL = _isRTL;

- (instancetype)initWithFrame:(NSRect)frameRect {
  self = [super initWithFrame:frameRect];
  if (self) {
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

- (NSRect)sharingTextRectForBounds:(NSRect)bounds {
  return ComputeBackdropContentLayout(bounds, _titleText, _sharingText,
                                      NSColor.whiteColor, _isRTL)
      .sharing_rect;
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

  BackdropContentLayout layout = ComputeBackdropContentLayout(
      bounds, _titleText, _sharingText, textColor, _isRTL);

  if (_iconImage) {
    [_iconImage drawInRect:layout.icon_rect
                  fromRect:NSZeroRect
                 operation:NSCompositingOperationSourceOver
                  fraction:1.0
            respectFlipped:YES
                     hints:nil];
  }

  if (_titleText.length > 0 && NSHeight(layout.title_rect) > 0.0) {
    [_titleText drawWithRect:layout.title_rect
                     options:NSStringDrawingUsesLineFragmentOrigin |
                             NSStringDrawingUsesFontLeading
                  attributes:layout.title_attrs];
  }

  if (_sharingText.length > 0 && NSHeight(layout.sharing_rect) > 0.0) {
    [_sharingText drawWithRect:layout.sharing_rect
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

  if (!UpdateBackdrops()) {
    return false;
  }

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

  local_input_monitor_.reset();

  for (PrivacyOverlayBackdropWindow* window in backdrop_windows_) {
    [window orderOut:nil];
    [window close];
  }
  [backdrop_windows_ removeAllObjects];
}

bool PrivacyOverlayMac::UpdateBackdrops() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

    [window orderFrontRegardless];
  }

  return true;
}

// static
std::unique_ptr<HostWindow> HostWindow::CreatePrivacyOverlay(
    std::unique_ptr<LocalInputMonitor> local_input_monitor) {
  return std::make_unique<PrivacyOverlayMac>(std::move(local_input_monitor));
}

}  // namespace remoting
