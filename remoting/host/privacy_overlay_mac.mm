// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "remoting/host/privacy_overlay_mac.h"

#import <Cocoa/Cocoa.h>
#include <CoreGraphics/CoreGraphics.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"
#include "remoting/host/input_monitor/local_input_monitor.h"

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

}  // namespace

@implementation PrivacyOverlayBackdropView {
  std::optional<bool> _highContrastOverrideForTesting;
}

- (BOOL)isFlipped {
  return YES;
}

- (BOOL)isOpaque {
  return YES;
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
  NSRect bounds = self.bounds;
  if (NSWidth(bounds) <= 0.0 || NSHeight(bounds) <= 0.0) {
    return;
  }

  NSColor* backgroundColor = [self shouldUseHighContrastColors]
                                 ? NSColor.windowBackgroundColor
                                 : [NSColor colorWithSRGBRed:kGoogleBlueRed
                                                       green:kGoogleBlueGreen
                                                        blue:kGoogleBlueBlue
                                                       alpha:1.0];
  [backgroundColor setFill];
  NSRectFill(dirtyRect);
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

  return UpdateBackdrops();
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

    PrivacyOverlayBackdropView* view =
        base::apple::ObjCCastStrict<PrivacyOverlayBackdropView>(
            window.contentView);
    NSRect view_frame =
        NSMakeRect(0, 0, NSWidth(screen_frame), NSHeight(screen_frame));
    if (!NSEqualRects(view.frame, view_frame)) {
      view.frame = view_frame;
      view.needsDisplay = YES;
    }

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
