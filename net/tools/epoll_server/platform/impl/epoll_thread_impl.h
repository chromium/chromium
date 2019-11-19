// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_THREAD_IMPL_H_
#define NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_THREAD_IMPL_H_

#include "base/threading/simple_thread.h"

namespace epoll_server {

// A class representing a thread of execution in epoll_server.
class EpollThreadImpl : public base::SimpleThread {
 public:
  EpollThreadImpl(const std::string& string) : base::SimpleThread(string) {}
  EpollThreadImpl(const EpollThreadImpl&) = delete;
  EpollThreadImpl& operator=(const EpollThreadImpl&) = delete;
};

}  // namespace epoll_server

#endif  // NET_TOOLS_EPOLL_SERVER_PLATFORM_IMPL_EPOLL_THREAD_IMPL_H_
