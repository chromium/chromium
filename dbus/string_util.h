// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_STRING_UTIL_H_
#define DBUS_STRING_UTIL_H_

#include <string>

#include "dbus/dbus_export.h"

namespace dbus {

// Returns true if the specified string is a valid object path.
CHROME_DBUS_EXPORT bool IsValidObjectPath(const std::string& value);

}  // namespace dbus

#endif  // DBUS_STRING_UTIL_H_
