// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_
#define REMOTING_HOST_DISCONNECT_WINDOW_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/weak_ptr.h"
#include "remoting/host/disconnect_window_base.h"

@class DisconnectWindowController;

namespace remoting {

class DisconnectWindowMac : public DisconnectWindowBase {
 public:
  DisconnectWindowMac();
  DisconnectWindowMac(const DisconnectWindowMac&) = delete;
  DisconnectWindowMac& operator=(const DisconnectWindowMac&) = delete;
  ~DisconnectWindowMac() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

  using DisconnectWindowBase::ResetRepositionAttempts;
  using DisconnectWindowBase::SetExpectedPosition;
  using DisconnectWindowBase::ShouldRepositionOnDisplacement;

 protected:
  void OnCooldownExpired() override;

 private:
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
