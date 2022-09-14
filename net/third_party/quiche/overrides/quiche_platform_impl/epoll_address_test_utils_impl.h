// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_ADDRESS_TEST_UTILS_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_ADDRESS_TEST_UTILS_IMPL_H_

#include <netinet/in.h>

namespace epoll_server {

int AddressFamilyUnderTestImpl() {
  return AF_INET;
}

}  // namespace epoll_server

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_ADDRESS_TEST_UTILS_IMPL_H_
