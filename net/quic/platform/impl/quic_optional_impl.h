// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_
#define NET_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_

#include "base/optional.h"

namespace quic {

template <typename T>
using QuicOptionalImpl = base::Optional<T>;

}  // namespace quic

#endif  // NET_QUIC_PLATFORM_IMPL_QUIC_OPTIONAL_IMPL_H_
