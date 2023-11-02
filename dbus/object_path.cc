// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_path.h"

#include <ostream>

#include "dbus/string_util.h"

namespace dbus {

bool ObjectPath::IsValid() const {
  return IsValidObjectPath(value_);
}

bool ObjectPath::operator<(const ObjectPath& that) const {
  return value_ < that.value_;
}

bool ObjectPath::operator==(const ObjectPath& that) const {
  return value_ == that.value_;
}

bool ObjectPath::operator!=(const ObjectPath& that) const {
  return value_ != that.value_;
}

void PrintTo(const ObjectPath& path, std::ostream* out) {
  *out << path.value();
}

} // namespace dbus
