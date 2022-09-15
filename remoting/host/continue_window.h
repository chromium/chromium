// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CONTINUE_WINDOW_H_
#define REMOTING_HOST_CONTINUE_WINDOW_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "remoting/host/host_window.h"

namespace remoting {

class ContinueWindow : public HostWindow {
 public:
  ContinueWindow(const ContinueWindow&) = delete;
  ContinueWindow& operator=(const ContinueWindow&) = delete;

  ~ContinueWindow() override;

  // HostWindow override.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

  // Resumes paused client session.
  void ContinueSession();

  // Disconnects the client session.
  void DisconnectSession();

 protected:
  ContinueWindow();

  // Shows and hides the UI.
  virtual void ShowUi() = 0;
  virtual void HideUi() = 0;

 private:
  // Invoked periodically to ask for the local user whether the session should
  // be continued.
  void OnSessionExpired();

  // Used to disconnect the client session.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Used to disconnect the client session when timeout expires.
  base::OneShotTimer disconnect_timer_;

  // Used to ask the local user whether the session should be continued.
  base::OneShotTimer session_expired_timer_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CONTINUE_WINDOW_H_
