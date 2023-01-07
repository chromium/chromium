// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_BUG_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_BUG_IMPL_H_

#include "quiche_platform_impl/epoll_logging_impl.h"

#define EPOLL_BUG_IMPL(bug_id) EPOLL_LOG_IMPL(DFATAL)
#define EPOLL_BUG_V2_IMPL(bug_id) EPOLL_LOG_IMPL(DFATAL)

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_EPOLL_BUG_IMPL_H_
