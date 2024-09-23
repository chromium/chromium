// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quiche_stack_trace_impl.h"

#include "base/containers/to_vector.h"

namespace quiche {
namespace {
static constexpr size_t kMaxStackSize = 256;
}  // namespace

std::vector<void*> CurrentStackTraceImpl() {
  std::vector<const void*> stacktrace(kMaxStackSize);
  size_t frame_count = base::debug::CollectStackTrace(stacktrace);
  if (frame_count <= 0) {
    return {};
  }
  stacktrace.resize(frame_count);
  return base::ToVector(stacktrace,
                        [](const void* p) { return const_cast<void*>(p); });
}

std::string SymbolizeStackTraceImpl(base::span<const void* const> stacktrace) {
  return base::debug::StackTrace(stacktrace).ToString();
}

std::string QuicheStackTraceImpl() {
  return base::debug::StackTrace().ToString();
}

bool QuicheShouldRunStackTraceTestImpl() {
  return base::debug::StackTrace::WillSymbolizeToStreamForTesting();
}

}  // namespace quiche
