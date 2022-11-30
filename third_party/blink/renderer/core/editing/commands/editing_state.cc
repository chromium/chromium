// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/editing_state.h"

#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

EditingState::EditingState() = default;

void EditingState::Abort() {
  DCHECK(!is_aborted_);
  is_aborted_ = true;
}

// ---
IgnorableEditingAbortState::IgnorableEditingAbortState() = default;

IgnorableEditingAbortState::~IgnorableEditingAbortState() = default;

#if DCHECK_IS_ON()
// ---

NoEditingAbortChecker::NoEditingAbortChecker(const char* file, int line)
    : file_(file), line_(line) {}

NoEditingAbortChecker::~NoEditingAbortChecker() {
  DCHECK_AT(!editing_state_.IsAborted(), file_, line_)
      << "The operation should not have been aborted.";
}

#endif

}  // namespace blink
