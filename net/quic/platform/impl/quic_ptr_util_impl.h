// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_PTR_UTIL_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_PTR_UTIL_IMPL_H_

#include "base/memory/ptr_util.h"

namespace quic {

template <typename T>
std::unique_ptr<T> QuicWrapUniqueImpl(T* ptr) {
  return base::WrapUnique<T>(ptr);
}

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_PTR_UTIL_IMPL_H_
