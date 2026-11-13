// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_
#define REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_

#import <Cocoa/Cocoa.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/host/disconnect_window_base.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/types/event_type.h"

@class PrivacyOverlayBackdropView;
@class PrivacyOverlayBackdropWindow;
@class PrivacyOverlayButtonView;
@class PrivacyOverlayButtonWindow;

namespace remoting {

class ClientSessionControl;
class LocalInputMonitor;

// Implements the macOS "Light Curtain" Privacy Overlay (`HostWindow`).
//
// Architecture:
// 1. Per-Display Backdrop Windows (`backdrop_windows_`):
//    One borderless, click-through (`ignoresMouseEvents = YES`),
//    capture-excluded (`NSWindowSharingNone` +
//    `CGSSetWindowCaptureExcludeShape`) backdrop window per active `NSScreen`
//    at `NSScreenSaverWindowLevel`.
// 2. On-Demand Floating Disconnect Button Window (`button_window_`):
//    A single borderless, non-activating (`NSPanel`), capture-excluded panel at
//    `NSScreenSaverWindowLevel + 1` wrapping the localized "Stop Sharing"
//    button. In its idle state, `button_window_` is ordered out (`orderOut:`,
//    removing it from the screen window list so `visible == NO`), and also
//    marked transparent (`alphaValue = 0`) and click-through
//    (`ignoresMouseEvents = YES`) as defense-in-depth across WindowServer
//    ordering transitions so remote clicks at the button's screen coordinates
//    pass through to underlying desktop applications. When local physical
//    pointer or keyboard input is detected via `local_input_monitor_`,
//    `button_window_` becomes visible and clickable on the target `NSScreen`
//    for `kAutoHideTimeout` (10s). Events synthesized by the CRD host process
//    (`kCGEventSourceUnixProcessID == getpid()`) are also ignored.
class PrivacyOverlayMac : public DisconnectWindowBase {
 public:
  // Returns active display frames in Cocoa global screen coordinates, with
  // index 0 guaranteed to be the primary (menu-bar) display (matching
  // `NSScreen.screens`).
  using ScreenFramesProvider = base::RepeatingCallback<std::vector<NSRect>()>;
  using CaptureExclusionApplier =
      base::RepeatingCallback<bool(NSWindow*, NSSize)>;
  using MouseLocationProvider = base::RepeatingCallback<NSPoint()>;

  explicit PrivacyOverlayMac(
      std::unique_ptr<LocalInputMonitor> local_input_monitor);

  PrivacyOverlayMac(const PrivacyOverlayMac&) = delete;
  PrivacyOverlayMac& operator=(const PrivacyOverlayMac&) = delete;

  ~PrivacyOverlayMac() override;

  // `HostWindow` implementation.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

  static base::AutoReset<ScreenFramesProvider>
  SetScreenFramesProviderForTesting(ScreenFramesProvider provider);

  static base::AutoReset<CaptureExclusionApplier>
  SetCaptureExclusionApplierForTesting(CaptureExclusionApplier applier);

  static base::AutoReset<MouseLocationProvider>
  SetMouseLocationProviderForTesting(MouseLocationProvider provider);

  static bool IsRemoteInjectedEventForTesting(NSEvent* event);

  NSArray<PrivacyOverlayBackdropWindow*>* backdrop_windows_for_testing() const {
    return backdrop_windows_;
  }

  bool UpdateBackdropsForTesting() { return UpdateBackdrops(); }

  PrivacyOverlayButtonWindow* button_window_for_testing() const {
    return button_window_;
  }

  bool is_button_hidden_for_testing() const { return is_button_hidden_; }

  bool is_rtl_for_testing() const { return is_rtl_; }

  void ActivateDisconnectButtonForTesting(NSPoint cocoa_screen_point) {
    ActivateDisconnectButton(cocoa_screen_point);
  }

 private:
  // Initializes localized strings, per-display backdrop windows, and the
  // floating Disconnect button window.
  bool BeginOverlay();

  // Closes and releases all overlay windows, stops the auto-hide timer, and
  // stops input monitoring.
  void EndOverlay();

  // Creates or updates one `PrivacyOverlayBackdropWindow` per active
  // `NSScreen`.
  bool UpdateBackdrops();

  // Positions `button_window_` horizontally centered and vertically below the
  // wrapped sharing message (when vertical screen space permits, clamping to
  // stay on-screen) on the display in `screen_frames` containing or nearest to
  // `cocoa_screen_point`.
  void PositionDisconnectButton(const std::vector<NSRect>& screen_frames,
                                NSPoint cocoa_screen_point);

  // Queries active screen frames and activates `button_window_` on the display
  // containing or nearest to `cocoa_screen_point` (in Cocoa screen coords).
  void ActivateDisconnectButton(NSPoint cocoa_screen_point);

  // Queries active screen frames once, converts `quartz_point` to Cocoa screen
  // coordinates, and activates `button_window_` on that display.
  void ActivateDisconnectButtonForQuartzPoint(
      const webrtc::DesktopVector& quartz_point);

  // Shows and enables click-targeting on `button_window_` on the display in
  // `screen_frames` containing or nearest to `cocoa_screen_point`, and
  // starts/resets `auto_hide_timer_` when `auto_hide_enabled_` is true.
  void ActivateDisconnectButtonWithFrames(
      const std::vector<NSRect>& screen_frames,
      NSPoint cocoa_screen_point);

  // Orders `button_window_` out (`orderOut:`) and marks it transparent and
  // click-through (`ignoresMouseEvents = YES`) when `auto_hide_timer_` expires.
  void DeactivateDisconnectButton();

  // Permanently reveals the Disconnect button if local input monitoring is
  // unavailable or encounters an error.
  void StopAutoHideBehavior();

  // Called when a local mouse event is observed by `local_input_monitor_`.
  // `position` is in Quartz global display coordinates (`(0, 0)` at top-left
  // of the primary screen).
  void OnLocalMouseEvent(const webrtc::DesktopVector& position,
                         ui::EventType type);

  // Called when a local keyboard event is observed by `local_input_monitor_`.
  void OnLocalKeyPressed(uint32_t usb_keycode);

  // Invoked when the local user clicks the Disconnect button.
  void OnDisconnectButtonClicked();

  // Monitors local hardware mouse and keyboard input to wake the Disconnect
  // button on demand.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Re-entrancy guard preventing `EndOverlay()` from running twice.
  bool is_ending_ = false;

  // True while the Disconnect button should auto-hide after
  // `kAutoHideTimeout`. Set to false by `StopAutoHideBehavior()` if
  // `local_input_monitor_` is unavailable or reports an error.
  bool auto_hide_enabled_ = true;

  // True when `button_window_` is ordered out in its idle state.
  bool is_button_hidden_ = true;

  // True if the localized message or process UI language uses a right-to-left
  // (RTL) reading layout.
  bool is_rtl_ = false;

  // Timer that returns the Disconnect button to its hidden click-through state
  // after `kAutoHideTimeout` of local input inactivity.
  base::OneShotTimer auto_hide_timer_;

  // Last observed local cursor position in Quartz global display coordinates.
  webrtc::DesktopVector mouse_position_;

  // Screen frame (in Cocoa global coordinates) on which `button_window_` was
  // most recently positioned, used to avoid redundant text layout and
  // displacement-counter resets when pointer events stay on the same display.
  std::optional<NSRect> active_button_screen_frame_;

  // Localized strings and icon displayed on the overlay windows.
  NSString* __strong product_name_text_;
  NSString* __strong sharing_with_text_;
  NSString* __strong button_text_;
  NSImage* __strong icon_image_;

  // Topmost click-through backdrop windows, one per active `NSScreen`.
  NSMutableArray<PrivacyOverlayBackdropWindow*>* __strong backdrop_windows_;

  // Floating Disconnect button window at `NSScreenSaverWindowLevel + 1`.
  PrivacyOverlayButtonWindow* __strong button_window_;

  base::WeakPtrFactory<PrivacyOverlayMac> weak_factory_{this};
};

}  // namespace remoting

// Custom `NSView` that renders the branded Google Blue (or High Contrast)
// privacy curtain with centered icon, localized product title, and localized
// sharing message.
@interface PrivacyOverlayBackdropView : NSView

@property(nonatomic, copy) NSString* titleText;
@property(nonatomic, copy) NSString* sharingText;
@property(nonatomic, strong) NSImage* iconImage;
@property(nonatomic, assign) BOOL isRTL;

- (void)setHighContrastOverrideForTesting:(std::optional<bool>)highContrast;

// Computes the bounding rectangle of the wrapped `sharingText` in the view's
// flipped coordinate system (`y = 0` at top) for `bounds`.
- (NSRect)sharingTextRectForBounds:(NSRect)bounds;

@end

// Borderless, click-through, capture-excluded `NSWindow` covering a single
// display frame.
@interface PrivacyOverlayBackdropWindow : NSWindow

- (instancetype)initWithScreenFrame:(NSRect)screenFrame;

@end

// Custom `NSView` that draws the rounded background and contrasting border of
// the floating Disconnect button window.
@interface PrivacyOverlayButtonView : NSView

- (void)setHighContrastOverrideForTesting:(std::optional<bool>)highContrast;

@end

// Floating, non-activating (`NSWindowStyleMaskNonactivatingPanel`),
// capture-excluded `NSPanel` hosting the "Stop Sharing" button above the
// backdrop windows.
@interface PrivacyOverlayButtonWindow : NSPanel

@property(nonatomic, strong, readonly) NSButton* disconnectButton;
@property(nonatomic, strong, readonly) PrivacyOverlayButtonView* buttonView;

- (instancetype)initWithTitle:(NSString*)buttonTitle
                  windowTitle:(NSString*)windowTitle
                        isRTL:(BOOL)isRTL
                 onDisconnect:(base::RepeatingClosure)onDisconnect;

@end

#endif  // REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_
