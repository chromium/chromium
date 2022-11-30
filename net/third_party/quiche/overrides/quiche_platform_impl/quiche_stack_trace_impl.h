// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_

#include "base/debug/stack_trace.h"

namespace quiche {

inline std::string QuicheStackTraceImpl() {
  return base::debug::StackTrace().ToString();
}

inline bool QuicheShouldRunStackTraceTestImpl() {
  return base::debug::StackTrace::WillSymbolizeToStreamForTesting();
}

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_
