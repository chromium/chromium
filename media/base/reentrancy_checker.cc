// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/reentrancy_checker.h"

namespace media {

NonReentrantScope::NonReentrantScope(base::Lock& lock) : lock_(lock) {
  is_lock_holder_ = lock_->Try();

  // TODO(sandersd): Allow the caller to provide the message? The macro knows
  // the name of the scope.
  if (!is_lock_holder_)
    LOG(FATAL) << "Non-reentrant scope was reentered";
}

NonReentrantScope::~NonReentrantScope() {
  if (!is_lock_holder_)
    return;

  lock_->AssertAcquired();
  lock_->Release();
}

}  // namespace media
