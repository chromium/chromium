// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/continue_window.h"

#include "base/location.h"
#include "base/time/time.h"
#include "remoting/host/client_session_control.h"

// Minutes before the local user should confirm that the session should go on.
constexpr base::TimeDelta kSessionExpirationTimeout = base::Minutes(30);

// Minutes before the session will be disconnected (from the moment the Continue
// window has been shown).
constexpr base::TimeDelta kSessionDisconnectTimeout = base::Minutes(5);

namespace remoting {

ContinueWindow::ContinueWindow() = default;

ContinueWindow::~ContinueWindow() = default;

void ContinueWindow::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_session_control_);
  DCHECK(client_session_control);

  client_session_control_ = client_session_control;

  session_expired_timer_.Start(FROM_HERE, kSessionExpirationTimeout, this,
                               &ContinueWindow::OnSessionExpired);
}

void ContinueWindow::ContinueSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  disconnect_timer_.Stop();

  if (!client_session_control_) {
    return;
  }

  // Hide the Continue window and resume the session.
  HideUi();
  client_session_control_->SetDisableInputs(false);

  session_expired_timer_.Start(FROM_HERE, kSessionExpirationTimeout, this,
                               &ContinueWindow::OnSessionExpired);
}

void ContinueWindow::DisconnectSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  disconnect_timer_.Stop();
  if (client_session_control_) {
    client_session_control_->DisconnectSession(ErrorCode::MAX_SESSION_LENGTH);
  }
}

void ContinueWindow::OnSessionExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!client_session_control_) {
    return;
  }

  // Stop the remote input while the Continue window is shown.
  client_session_control_->SetDisableInputs(true);

  // Show the Continue window and wait for the local user input.
  ShowUi();
  disconnect_timer_.Start(FROM_HERE, kSessionDisconnectTimeout, this,
                          &ContinueWindow::DisconnectSession);
}

}  // namespace remoting
