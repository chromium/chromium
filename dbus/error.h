// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_ERROR_H_
#define DBUS_ERROR_H_

#include <string>

#include "dbus/dbus_export.h"

namespace dbus {

// Represents D-Bus related errors.
// This carries error info retrieved from libdbus. Some APIs under dbus/
// may return empty Error instance to represent the API failed, but not
// from libdbus.
class CHROME_DBUS_EXPORT Error {
 public:
  // Creates an invalid error.
  Error();

  // Creates an error instance with the given name and the message.
  Error(std::string name, std::string message);
  Error(Error&& other);
  Error& operator=(Error&& other);
  ~Error();

  // Returns true if the error is valid one.
  bool IsValid() const { return !name_.empty(); }

  // Returns the name of the D-Bus error.
  // Please see also "Error names" in
  // https://dbus.freedesktop.org/doc/dbus-specification.html#message-protocol-names
  // Specifically, valid name must have two components connected by '.', so this
  // class uses empty name to represent invalid error instance.
  const std::string& name() const { return name_; }

  // Returns (human readable) error message attached to D-Bus error.
  const std::string& message() const { return message_; }

 private:
  std::string name_;
  std::string message_;
};

}  // namespace dbus

#endif  // DBUS_ERROR_H_
