// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/scoped_dbus_error.h"

namespace dbus::internal {

ScopedDBusError::ScopedDBusError() {
  dbus_error_init(&error_);
}

ScopedDBusError::~ScopedDBusError() {
  dbus_error_free(&error_);
}

bool ScopedDBusError::is_set() const {
  return dbus_error_is_set(&error_);
}

}  // namespace dbus::internal
