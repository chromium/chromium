// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_INSTALL_WARNING_H_
#define EXTENSIONS_COMMON_INSTALL_WARNING_H_

#include "base/macros.h"

#include <ostream>
#include <string>

namespace extensions {

// A struct to describe a non-fatal issue discovered in the installation of an
// extension.
struct InstallWarning {
  explicit InstallWarning(const std::string& message);
  InstallWarning(const std::string& message,
                 const std::string& key);
  InstallWarning(const std::string& message,
                 const std::string& key,
                 const std::string& specific);
  InstallWarning(InstallWarning&& other);
  InstallWarning& operator=(InstallWarning&& other);
  ~InstallWarning();

  bool operator==(const InstallWarning& other) const {
    // We don't have to look at |key| or |specific| here, because they are each
    // used in the the message itself.
    // For example, a full message would be "Permission 'foo' is unknown or URL
    // pattern is malformed." |key| here is "permissions", and |specific| is
    // "foo", but these are redundant with the message.
    return message == other.message;
  }

  // The warning's message (human-friendly).
  std::string message;
  // Optional - for specifying the incorrect key in the manifest (e.g.,
  // "permissions").
  std::string key;
  // Optional - for specifying the incorrect portion of a key in the manifest
  // (e.g., an unrecognized permission "foo" in "permissions").
  std::string specific;

  DISALLOW_COPY(InstallWarning);
};

// Let gtest print InstallWarnings.
void PrintTo(const InstallWarning&, ::std::ostream* os);

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_INSTALL_WARNING_H_
