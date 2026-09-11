// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window_base.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "remoting/base/email_utils.h"
#include "remoting/protocol/errors.h"

namespace remoting {

namespace {

// Remembers the last selected anchor position across dialog instances.
DisconnectWindowBase::WindowAnchor g_current_anchor =
    DisconnectWindowBase::WindowAnchor::kBottom;

}  // namespace

DisconnectWindowBase::DisconnectWindowBase() = default;

DisconnectWindowBase::~DisconnectWindowBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cooldown_timer_.Stop();
}

void DisconnectWindowBase::Start(
    const base::WeakPtr<ClientSessionControl>& client_session_control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!client_session_control_);
  DCHECK(client_session_control);

  client_session_control_ = client_session_control;
  client_jid_ = client_session_control_->client_jid();
  email_ = client_jid_.substr(0, client_jid_.find('/'));
  formatted_email_ = FormatEmailForDisplay(email_);
  expected_x_.reset();
  expected_y_.reset();
  consecutive_reposition_attempts_ = 0;
}

void DisconnectWindowBase::DisconnectSession(std::string_view reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_session_control_) {
    client_session_control_->DisconnectSession(protocol::ErrorCode::OK, reason,
                                               FROM_HERE);
  }
}

void DisconnectWindowBase::ToggleAlignment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  g_current_anchor = (g_current_anchor == WindowAnchor::kBottom)
                         ? WindowAnchor::kTop
                         : WindowAnchor::kBottom;
  consecutive_reposition_attempts_ = 0;
  if (!cooldown_timer_.IsRunning()) {
    cooldown_timer_.Start(
        FROM_HERE, kToggleCooldown,
        base::BindOnce(&DisconnectWindowBase::HandleCooldownExpired,
                       weak_factory_.GetWeakPtr()));
  }
}

void DisconnectWindowBase::OnCooldownExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void DisconnectWindowBase::HandleCooldownExpired() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnCooldownExpired();
}

void DisconnectWindowBase::SetExpectedPosition(int x, int y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  expected_x_ = x;
  expected_y_ = y;
}

bool DisconnectWindowBase::ShouldRepositionOnDisplacement(int actual_x,
                                                          int actual_y) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!expected_x_.has_value() || !expected_y_.has_value()) {
    return false;
  }
  if (actual_x == *expected_x_ && actual_y == *expected_y_) {
    consecutive_reposition_attempts_ = 0;
    return false;
  }
  if (consecutive_reposition_attempts_ < kMaxRepositionAttempts) {
    ++consecutive_reposition_attempts_;
    return true;
  }
  return false;
}

void DisconnectWindowBase::ResetRepositionAttempts() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  consecutive_reposition_attempts_ = 0;
}

DisconnectWindowBase::WindowAnchor DisconnectWindowBase::current_anchor()
    const {
  return g_current_anchor;
}

bool DisconnectWindowBase::is_cooldown_active() const {
  return cooldown_timer_.IsRunning();
}

const std::string& DisconnectWindowBase::client_jid() const {
  return client_jid_;
}

const std::string& DisconnectWindowBase::email() const {
  return email_;
}

const std::u16string& DisconnectWindowBase::formatted_email() const {
  return formatted_email_;
}

std::optional<int> DisconnectWindowBase::expected_x() const {
  return expected_x_;
}

std::optional<int> DisconnectWindowBase::expected_y() const {
  return expected_y_;
}

int DisconnectWindowBase::consecutive_reposition_attempts() const {
  return consecutive_reposition_attempts_;
}

const base::WeakPtr<ClientSessionControl>&
DisconnectWindowBase::client_session_control() const {
  return client_session_control_;
}

// static
void DisconnectWindowBase::ResetCurrentAnchorForTesting() {
  g_current_anchor = WindowAnchor::kBottom;
}

}  // namespace remoting
