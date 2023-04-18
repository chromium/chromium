// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_
#define NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_

#include <linux/rtnetlink.h>

bool operator==(const struct ifaddrmsg& lhs, const struct ifaddrmsg& rhs);

#endif  // NET_BASE_ADDRESS_TRACKER_LINUX_TEST_UTIL_H_
