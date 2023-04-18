// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_tracker_linux_test_util.h"

#include <linux/rtnetlink.h>
#include <string.h>

bool operator==(const struct ifaddrmsg& lhs, const struct ifaddrmsg& rhs) {
  return memcmp(&lhs, &rhs, sizeof(struct ifaddrmsg)) == 0;
}
