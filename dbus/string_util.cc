// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/string_util.h"

#include <stddef.h>

#include "base/strings/string_util.h"

namespace dbus {

// This implementation is based upon D-Bus Specification Version 0.19.
bool IsValidObjectPath(const std::string& value) {
  // A valid object path begins with '/'.
  if (!base::StartsWith(value, "/", base::CompareCase::SENSITIVE))
    return false;

  // Elements are pieces delimited by '/'. For instance, "org", "chromium",
  // "Foo" are elements of "/org/chromium/Foo".
  int element_length = 0;
  for (size_t i = 1; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '/') {
      // No element may be the empty string.
      if (element_length == 0)
        return false;
      element_length = 0;
    } else {
      // Each element must only contain "[A-Z][a-z][0-9]_".
      const bool is_valid_character =
          ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') ||
          ('0' <= c && c <= '9') || c == '_';
      if (!is_valid_character)
        return false;
      element_length++;
    }
  }

  // A trailing '/' character is not allowed unless the path is the root path.
  if (value.size() > 1 &&
      base::EndsWith(value, "/", base::CompareCase::SENSITIVE))
    return false;

  return true;
}

}  // namespace dbus
