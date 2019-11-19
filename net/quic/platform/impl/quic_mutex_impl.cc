// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/platform/impl/quic_mutex_impl.h"

namespace quic {

void QuicLockImpl::WriterLock() {
  lock_.Acquire();
}

void QuicLockImpl::WriterUnlock() {
  lock_.Release();
}

void QuicLockImpl::ReaderLock() {
  lock_.Acquire();
}

void QuicLockImpl::ReaderUnlock() {
  lock_.Release();
}

}  // namespace quic
