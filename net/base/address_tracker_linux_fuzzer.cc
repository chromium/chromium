// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/functional/callback_helpers.h"
#include "net/base/address_tracker_linux.h"

using net::internal::AddressTrackerLinux;

namespace net::test {

class AddressTrackerLinuxTest {
 public:
  static void TestHandleMessage(const char* buffer, size_t length) {
    std::unordered_set<std::string> ignored_interfaces;
    AddressTrackerLinux tracker(base::DoNothing(), base::DoNothing(),
                                base::DoNothing(), ignored_interfaces);
    bool address_changed, link_changed, tunnel_changed;
    tracker.HandleMessage(buffer, length, &address_changed, &link_changed,
                          &tunnel_changed);
  }
};

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0)
    return 0;
  AddressTrackerLinuxTest::TestHandleMessage(
      reinterpret_cast<const char*>(data), size);
  return 0;
}

}  // namespace net::test
