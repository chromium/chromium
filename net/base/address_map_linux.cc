// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_map_linux.h"

#include <linux/rtnetlink.h>

namespace net {

internal::AddressTrackerLinux* AddressMapOwnerLinux::GetAddressTrackerLinux() {
  return nullptr;
}
AddressMapCacheLinux* AddressMapOwnerLinux::GetAddressMapCacheLinux() {
  return nullptr;
}

}  // namespace net
