// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/utility/utility_thread.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"

namespace content {

// Keep the global UtilityThread in a TLS slot so it is impossible to access
// incorrectly from the wrong thread.
static base::LazyInstance<base::ThreadLocalPointer<UtilityThread>>::Leaky
    lazy_tls = LAZY_INSTANCE_INITIALIZER;

UtilityThread* UtilityThread::Get() {
  return lazy_tls.Pointer()->Get();
}

UtilityThread::UtilityThread() {
  lazy_tls.Pointer()->Set(this);
}

UtilityThread::~UtilityThread() {
  lazy_tls.Pointer()->Set(nullptr);
}

}  // namespace content

