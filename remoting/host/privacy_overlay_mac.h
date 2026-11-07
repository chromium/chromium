// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_
#define REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "remoting/host/disconnect_window_base.h"

@class PrivacyOverlayBackdropView;
@class PrivacyOverlayBackdropWindow;

namespace remoting {

class ClientSessionControl;
class LocalInputMonitor;

// Implements the macOS "Light Curtain" Privacy Overlay (`HostWindow`).
//
// Creates one borderless, click-through (`ignoresMouseEvents = YES`),
// capture-excluded (`NSWindowSharingNone` + `CGSSetWindowCaptureExcludeShape`)
// backdrop window per active `NSScreen` at `NSScreenSaverWindowLevel` so local
// physical displays show the branded Google Blue privacy screen while
// ScreenCaptureKit / CGWindowList desktop capture sees through to the
// underlying desktop.
class PrivacyOverlayMac : public DisconnectWindowBase {
 public:
  using ScreenFramesProvider = base::RepeatingCallback<std::vector<NSRect>()>;
  using CaptureExclusionApplier =
      base::RepeatingCallback<bool(NSWindow*, NSSize)>;

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

  NSArray<PrivacyOverlayBackdropWindow*>* backdrop_windows_for_testing() const {
    return backdrop_windows_;
  }

  bool UpdateBackdropsForTesting() { return UpdateBackdrops(); }

 private:
  // Creates per-display backdrop windows.
  bool BeginOverlay();

  // Closes and releases all overlay windows and stops input monitoring.
  void EndOverlay();

  // Creates or updates one `PrivacyOverlayBackdropWindow` per active
  // `NSScreen`.
  bool UpdateBackdrops();

  // Monitors local hardware mouse and keyboard input (wired up in follow-up
  // CLs to reveal the floating Disconnect button on demand).
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  // Re-entrancy guard preventing `EndOverlay()` from running twice.
  bool is_ending_ = false;

  // Topmost click-through backdrop windows, one per active `NSScreen`.
  NSMutableArray<PrivacyOverlayBackdropWindow*>* __strong backdrop_windows_;
};

}  // namespace remoting

// Custom `NSView` that renders the branded Google Blue (or High Contrast)
// privacy curtain background.
@interface PrivacyOverlayBackdropView : NSView

- (void)setHighContrastOverrideForTesting:(std::optional<bool>)highContrast;

@end

// Borderless, click-through, capture-excluded `NSWindow` covering a single
// display frame.
@interface PrivacyOverlayBackdropWindow : NSWindow

- (instancetype)initWithScreenFrame:(NSRect)screenFrame;

@end

#endif  // REMOTING_HOST_PRIVACY_OVERLAY_MAC_H_
