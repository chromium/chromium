// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/overrides/quiche_platform_impl/quiche_mutex_impl.h"

namespace quiche {

void QuicheLockImpl::WriterLock() {
  lock_.Acquire();
}

void QuicheLockImpl::WriterUnlock() {
  lock_.Release();
}

void QuicheLockImpl::ReaderLock() {
  lock_.Acquire();
}

void QuicheLockImpl::ReaderUnlock() {
  lock_.Release();
}

}  // namespace quiche
