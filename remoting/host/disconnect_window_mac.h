// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_
#define REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/host/disconnect_window_base.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "ui/events/types/event_type.h"

@class DisconnectWindowController;

namespace remoting {

class LocalInputMonitor;

class DisconnectWindowMac : public DisconnectWindowBase {
 public:
  DisconnectWindowMac();
  DisconnectWindowMac(const DisconnectWindowMac&) = delete;
  DisconnectWindowMac& operator=(const DisconnectWindowMac&) = delete;
  ~DisconnectWindowMac() override;

  // Allow dialog to auto-hide after a period of time.  The dialog will be
  // reshown when local user input is detected.
  void EnableAutoHide(std::unique_ptr<LocalInputMonitor> local_input_monitor);

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

  using DisconnectWindowBase::ResetRepositionAttempts;
  using DisconnectWindowBase::SetExpectedPosition;
  using DisconnectWindowBase::ShouldRepositionOnDisplacement;

 protected:
  void OnCooldownExpired() override;

 private:
  // Shows a previously hidden dialog using an animation.
  void ShowDialog();

  // Hides the dialog using an animation.
  void HideDialog();

  // Prevent the dialog from being hidden if local input monitoring fails.
  void StopAutoHideBehavior();

  // Called when local mouse event is seen and shows the dialog (if hidden).
  void OnLocalMouseEvent(const webrtc::DesktopVector& mouse_position,
                         ui::EventType type);

  // Called when local keyboard event is seen and shows the dialog (if hidden).
  void OnLocalKeyPressed(uint32_t usb_keycode);

  // Used to watch for local input which will trigger the dialog to be reshown.
  std::unique_ptr<LocalInputMonitor> local_input_monitor_;

  bool was_auto_hidden_ = false;
  base::OneShotTimer auto_hide_timer_;
  webrtc::DesktopVector mouse_position_;

  DisconnectWindowController* __strong window_controller_;
  base::WeakPtrFactory<DisconnectWindowMac> weak_factory_{this};
};

}  // namespace remoting

// Controller for the disconnect window which allows the host user to
// quickly disconnect a session.
@interface DisconnectWindowController : NSWindowController

- (instancetype)initWithDisconnectWindow:
                    (base::WeakPtr<remoting::DisconnectWindowMac>)
                        disconnect_window
                                  window:(NSWindow*)window;
- (void)initializeWindow;
- (void)stopSharing:(id)sender;
- (void)onCooldownExpired;
- (void)hideDialog;
- (void)showDialog;
@end

// A floating window with a custom border. The custom border and background
// content is defined by DisconnectView. Declared here so that it can be
// instantiated via a xib.
@interface DisconnectWindow : NSWindow
@end

// The custom background/border for the DisconnectWindow. Declared here so that
// it can be instantiated via a xib.
@interface DisconnectView : NSView
@end

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_
