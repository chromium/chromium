// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/debug/stack_trace.h"
#include "quiche/common/platform/api/quiche_export.h"

namespace quiche {

QUICHE_EXPORT std::vector<void*> CurrentStackTraceImpl();
QUICHE_EXPORT std::string SymbolizeStackTraceImpl(
    base::span<const void* const> stacktrace);
QUICHE_EXPORT std::string QuicheStackTraceImpl();
QUICHE_EXPORT bool QuicheShouldRunStackTraceTestImpl();

}  // namespace quiche

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_STACK_TRACE_IMPL_H_
