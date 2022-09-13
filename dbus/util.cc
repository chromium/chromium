// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/util.h"

namespace dbus {

std::string GetAbsoluteMemberName(const std::string& interface_name,
                                  const std::string& member_name) {
  return interface_name + "." + member_name;
}

}  // namespace dbus
