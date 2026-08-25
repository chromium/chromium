// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/install_warning.h"

namespace extensions {

InstallWarning::InstallWarning(std::string_view message) : message(message) {}

InstallWarning::InstallWarning(std::string_view message, std::string_view key)
    : message(message), key(key) {}

InstallWarning::InstallWarning(std::string_view message,
                               std::string_view key,
                               std::string_view specific)
    : message(message), key(key), specific(specific) {}

InstallWarning::InstallWarning(InstallWarning&& other) = default;

InstallWarning& InstallWarning::operator=(InstallWarning&& other) = default;

InstallWarning::~InstallWarning() {
}

void PrintTo(const InstallWarning& warning, ::std::ostream* os) {
  // This is just for test error messages, so no need to escape '"'
  // characters inside the message.
  *os << "InstallWarning(\"" << warning.message << "\")";
}

}  // namespace extensions
