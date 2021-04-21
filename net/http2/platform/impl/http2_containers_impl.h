// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP2_PLATFORM_IMPL_HTTP2_CONTAINERS_IMPL_H_
#define NET_HTTP2_PLATFORM_IMPL_HTTP2_CONTAINERS_IMPL_H_

#include "base/containers/circular_deque.h"

namespace http2 {

template <typename T>
using Http2DequeImpl = base::circular_deque<T>;

}

#endif  // NET_HTTP2_PLATFORM_IMPL_HTTP2_CONTAINERS_IMPL_H_
