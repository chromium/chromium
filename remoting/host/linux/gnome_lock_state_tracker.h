// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GNOME_LOCK_STATE_TRACKER_H_
#define REMOTING_HOST_LINUX_GNOME_LOCK_STATE_TRACKER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/base/loggable.h"
#include "remoting/host/linux/gdbus_connection_ref.h"
#include "remoting/host/linux/gvariant_ref.h"
#include "remoting/host/linux/lock_state_tracker.h"

namespace remoting {

class GnomeLockStateTracker : public LockStateTracker {
 public:
  GnomeLockStateTracker(
      GDBusConnectionRef connection,
      gvariant::ObjectPath session_path,
      const char* bus_name_for_testing = "org.gnome.Mutter.RemoteDesktop");
  ~GnomeLockStateTracker() override;

  GnomeLockStateTracker(const GnomeLockStateTracker&) = delete;
  GnomeLockStateTracker& operator=(const GnomeLockStateTracker&) = delete;

  // LockStateTracker implementation.
  void Start() override;
  bool GetCapsLockState() const override;
  bool GetNumLockState() const override;
  void SetExpectedCapsLockState(bool state) override;
  void SetExpectedNumLockState(bool state) override;

 private:
  void OnGotCapsLockState(base::expected<bool, Loggable> result);
  void OnGotNumLockState(base::expected<bool, Loggable> result);
  void OnPropertiesChanged(gvariant::GVariantRef<"r"> properties);

  GDBusConnectionRef connection_;
  gvariant::ObjectPath session_path_;
  std::string bus_name_;

  bool caps_lock_state_ = false;
  bool num_lock_state_ = false;

  std::unique_ptr<GDBusConnectionRef::SignalSubscription>
      properties_changed_subscription_;

  base::WeakPtrFactory<GnomeLockStateTracker> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GNOME_LOCK_STATE_TRACKER_H_
