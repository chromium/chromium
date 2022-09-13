// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_OBJECT_PATH_H_
#define DBUS_OBJECT_PATH_H_

#include <iosfwd>
#include <string>

#include "dbus/dbus_export.h"

namespace dbus {

// ObjectPath is a type used to distinguish D-Bus object paths from simple
// strings, especially since normal practice is that these should be only
// initialized from static constants or obtained from remote objects and no
// assumptions about their value made.
class CHROME_DBUS_EXPORT ObjectPath {
 public:
  // Permit initialization without a value for passing to
  // dbus::MessageReader::PopObjectPath to fill in and from std::string
  // objects.
  //
  // The compiler synthesised copy constructor and assignment operator are
  // sufficient for our needs, as is implicit initialization of a std::string
  // from a string constant.
  ObjectPath() {}
  explicit ObjectPath(const std::string& value) : value_(value) {}

  // Retrieves value as a std::string.
  const std::string& value() const { return value_; }

  // Returns true if the value is a valid object path.
  bool IsValid() const;

  // Permit sufficient comparison to allow an ObjectPath to be used as a
  // key in a std::map.
  bool operator<(const ObjectPath&) const;

  // Permit testing for equality, required for mocks to work and useful for
  // observers.
  bool operator==(const ObjectPath&) const;
  bool operator!=(const ObjectPath&) const;

 private:
  std::string value_;
};

// This is required by gtest to print a readable output on test failures.
CHROME_DBUS_EXPORT void PrintTo(const ObjectPath& path, std::ostream* out);

}  // namespace dbus

#endif  // DBUS_OBJECT_PATH_H_
