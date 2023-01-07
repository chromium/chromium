// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/scheduler/common/cancelable_closure_holder.h"

namespace blink {
namespace scheduler {

CancelableClosureHolder::CancelableClosureHolder() = default;

CancelableClosureHolder::~CancelableClosureHolder() = default;

void CancelableClosureHolder::Reset(const base::RepeatingClosure& callback) {
  callback_ = callback;
  cancelable_callback_.Reset(callback_);
}

void CancelableClosureHolder::Cancel() {
  DCHECK(!callback_.is_null());
  cancelable_callback_.Reset(callback_);
}

base::RepeatingClosure CancelableClosureHolder::GetCallback() const {
  DCHECK(!callback_.is_null());
  return cancelable_callback_.callback();
}

}  // namespace scheduler
}  // namespace blink
