// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_SCOPED_DBUS_ERROR_H_
#define DBUS_SCOPED_DBUS_ERROR_H_

#include <dbus/dbus.h>

#include "dbus/dbus_export.h"

namespace dbus {

// Utility class to ensure that DBusError is freed.
class CHROME_DBUS_EXPORT ScopedDBusError {
 public:
  // Do not inline methods that call dbus_error_xxx() functions.
  // See http://crbug.com/416628
  ScopedDBusError();
  ~ScopedDBusError();

  DBusError* get() { return &error_; }
  bool is_set() const;
  const char* name() { return error_.name; }
  const char* message() { return error_.message; }

 private:
  DBusError error_;
};

}  // namespace dbus

#endif  // DBUS_SCOPED_DBUS_ERROR_H_
