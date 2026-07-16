// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"

#include "base/compiler_specific.h"

namespace blink {

LazyStackTrace::LazyStackTrace(v8::Isolate* isolate) : isolate_(isolate) {}
LazyStackTrace::LazyStackTrace(LazyStackTrace&& other) noexcept
    : isolate_(other.isolate_),
      cached_stack_(std::move(other.cached_stack_)),
      max_walked_limit_(other.max_walked_limit_) {
  other.isolate_ = nullptr;
  other.max_walked_limit_ = 0;
}
base::span<const v8::StackTrace::ScriptData> LazyStackTrace::GetStack(
    size_t limit) {
  if (!isolate_ || limit == 0) {
    return {};
  }

  if (limit <= max_walked_limit_) {
    return base::span(cached_stack_)
        .first(std::min(static_cast<wtf_size_t>(limit), cached_stack_.size()));
  }

  if (max_walked_limit_ > 0 && cached_stack_.size() < max_walked_limit_) {
    return cached_stack_;
  }

  max_walked_limit_ = limit;
  cached_stack_.resize(static_cast<wtf_size_t>(limit));
  base::span<v8::StackTrace::ScriptData> stack =
      v8::StackTrace::CurrentScriptData(isolate_, cached_stack_);

  if (stack.size() < limit) {
    cached_stack_.Shrink(static_cast<wtf_size_t>(stack.size()));
  }

  return cached_stack_;
}

}  // namespace blink
