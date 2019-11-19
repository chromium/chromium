// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#ifndef NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_PTR_UTIL_IMPL_H_
#define NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_PTR_UTIL_IMPL_H_

namespace epoll_server {

template <typename T, typename... Args>
std::unique_ptr<T> EpollMakeUniqueImpl(Args&&... args) {
  return std::make_unique<T>(std::forward<Args>(args)...);
}

}  // namespace epoll_server

#endif  // NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_PTR_UTIL_IMPL_H_
