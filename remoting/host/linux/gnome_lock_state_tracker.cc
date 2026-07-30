// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gnome_lock_state_tracker.h"

#include "base/functional/bind.h"
#include "remoting/base/logging.h"
#include "remoting/host/linux/dbus_interfaces/org_freedesktop_DBus_Properties.h"
#include "remoting/host/linux/dbus_interfaces/org_gnome_Mutter_RemoteDesktop.h"

namespace remoting {

GnomeLockStateTracker::GnomeLockStateTracker(GDBusConnectionRef connection,
                                             gvariant::ObjectPath session_path,
                                             const char* bus_name_for_testing)
    : connection_(connection),
      session_path_(session_path),
      bus_name_(bus_name_for_testing) {
  DCHECK(connection_.is_initialized());
  DCHECK(!session_path_.value().empty());
}

GnomeLockStateTracker::~GnomeLockStateTracker() = default;

void GnomeLockStateTracker::Start() {
  connection_
      .GetProperty<org_gnome_Mutter_RemoteDesktop_Session::CapsLockState>(
          bus_name_.c_str(), session_path_,
          base::BindOnce(&GnomeLockStateTracker::OnGotCapsLockState,
                         weak_factory_.GetWeakPtr()));
  connection_.GetProperty<org_gnome_Mutter_RemoteDesktop_Session::NumLockState>(
      bus_name_.c_str(), session_path_,
      base::BindOnce(&GnomeLockStateTracker::OnGotNumLockState,
                     weak_factory_.GetWeakPtr()));
  properties_changed_subscription_ =
      connection_
          .SignalSubscribe<org_freedesktop_DBus_Properties::PropertiesChanged>(
              bus_name_.c_str(), session_path_,
              base::BindRepeating(&GnomeLockStateTracker::OnPropertiesChanged,
                                  weak_factory_.GetWeakPtr()));
}

bool GnomeLockStateTracker::GetCapsLockState() const {
  return caps_lock_state_;
}

bool GnomeLockStateTracker::GetNumLockState() const {
  return num_lock_state_;
}

void GnomeLockStateTracker::SetExpectedCapsLockState(bool state) {
  caps_lock_state_ = state;
}

void GnomeLockStateTracker::SetExpectedNumLockState(bool state) {
  num_lock_state_ = state;
}

void GnomeLockStateTracker::OnGotCapsLockState(
    base::expected<bool, Loggable> result) {
  if (result.has_value()) {
    caps_lock_state_ = *result;
  } else {
    LOG(ERROR) << "Failed to get CapsLock State: " << result.error();
  }
}

void GnomeLockStateTracker::OnGotNumLockState(
    base::expected<bool, Loggable> result) {
  if (result.has_value()) {
    num_lock_state_ = *result;
  } else {
    LOG(ERROR) << "Failed to get NumLock State: " << result.error();
  }
}

void GnomeLockStateTracker::OnPropertiesChanged(
    gvariant::GVariantRef<"r"> properties) {
  auto properties_typed = properties.TryInto<gvariant::GVariantRef<
      org_freedesktop_DBus_Properties::PropertiesChanged::kType>>();
  if (!properties_typed.has_value()) {
    return;
  }
  auto [interface_name, changed_properties, invalidated_properties] =
      *properties_typed;
  if (interface_name.string_view() ==
      org_gnome_Mutter_RemoteDesktop_Session::CapsLockState::kInterfaceName) {
    auto capslock_variant = changed_properties.LookUp("CapsLockState");
    if (capslock_variant.has_value()) {
      auto unboxed_property =
          capslock_variant->TryInto<gvariant::Boxed<bool>>();
      if (unboxed_property.has_value()) {
        caps_lock_state_ = unboxed_property->value;
      }
    }
    auto numlock_variant = changed_properties.LookUp("NumLockState");
    if (numlock_variant.has_value()) {
      auto unboxed_property = numlock_variant->TryInto<gvariant::Boxed<bool>>();
      if (unboxed_property.has_value()) {
        num_lock_state_ = unboxed_property->value;
      }
    }
  }
}

}  // namespace remoting
