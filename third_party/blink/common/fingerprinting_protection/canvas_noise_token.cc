// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/fingerprinting_protection/canvas_noise_token.h"

#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"

namespace blink {

namespace {
// Protect |noise_token|. |noise_token| is written by the main renderer thread
// and is read by the main or worker thread(s).
static base::LazyInstance<base::Lock>::Leaky g_lock = LAZY_INSTANCE_INITIALIZER;
uint64_t noise_token GUARDED_BY(g_lock.Get()){0};

// For debugging, bool that indicates the token has been initialized.
#if DCHECK_IS_ON()
bool initialized = false;
#endif
}  // namespace

// static
void CanvasNoiseToken::Set(uint64_t token) {
  base::AutoLock lock(g_lock.Get());
  noise_token = token;
#if DCHECK_IS_ON()
  initialized = true;
#endif
}

// static
uint64_t CanvasNoiseToken::Get() {
  base::AutoLock lock(g_lock.Get());
#if DCHECK_IS_ON()
  DCHECK(initialized);
#endif
  return noise_token;
}

}  // namespace blink
