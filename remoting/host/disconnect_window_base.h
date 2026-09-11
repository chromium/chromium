// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DISCONNECT_WINDOW_BASE_H_
#define REMOTING_HOST_DISCONNECT_WINDOW_BASE_H_

#include <optional>
#include <string>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/host_window.h"

namespace remoting {

// Base class providing common alignment, cooldown timer, email extraction,
// disconnect session handling, and repositioning loop-prevention state
// shared by platform-specific disconnect window implementations.
class DisconnectWindowBase : public HostWindow {
 public:
  enum class WindowAnchor {
    kBottom,
    kTop,
  };

  // The amount of time to wait before allowing another position toggle.
  static constexpr base::TimeDelta kToggleCooldown = base::Seconds(3);

  // Maximum consecutive reposition attempts to prevent an infinite loop if the
  // window manager persistently denies or alters window positioning.
  static constexpr int kMaxRepositionAttempts = 3;

  static constexpr char kDisconnectClickedReason[] =
      "Disconnect button was clicked.";
  static constexpr char kDisconnectDeletedReason[] =
      "Disconnect window deleted.";

  DisconnectWindowBase();
  DisconnectWindowBase(const DisconnectWindowBase&) = delete;
  DisconnectWindowBase& operator=(const DisconnectWindowBase&) = delete;
  ~DisconnectWindowBase() override;

  // HostWindow overrides.
  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override;

  // Disconnects the remote session if active.
  void DisconnectSession(std::string_view reason = kDisconnectClickedReason);

  // Toggles dialog alignment between kBottom and kTop, and triggers cooldown.
  void ToggleAlignment();

  // Reset the anchor across instances (useful for testing).
  static void ResetCurrentAnchorForTesting();

  // Getters:
  WindowAnchor current_anchor() const;
  bool is_cooldown_active() const;
  const std::string& client_jid() const;
  const std::string& email() const;
  const std::u16string& formatted_email() const;
  std::optional<int> expected_x() const;
  std::optional<int> expected_y() const;
  int consecutive_reposition_attempts() const;
  const base::WeakPtr<ClientSessionControl>& client_session_control() const;

 protected:
  // Hook called when the position toggle cooldown timer expires. Subclasses
  // should override this to re-enable their UI toggle buttons.
  virtual void OnCooldownExpired();

  // Track expected window coordinates and evaluate whether displacement should
  // trigger snap-back repositioning.
  void SetExpectedPosition(int x, int y);
  bool ShouldRepositionOnDisplacement(int actual_x, int actual_y);
  void ResetRepositionAttempts();

 private:
  void HandleCooldownExpired();

  base::WeakPtr<ClientSessionControl> client_session_control_;

  std::string client_jid_;
  std::string email_;
  std::u16string formatted_email_;

  base::OneShotTimer cooldown_timer_;

  std::optional<int> expected_x_;
  std::optional<int> expected_y_;
  int consecutive_reposition_attempts_ = 0;

  base::WeakPtrFactory<DisconnectWindowBase> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DISCONNECT_WINDOW_BASE_H_
