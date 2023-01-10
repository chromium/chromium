// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pseudonymization_salt.h"

#include <atomic>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"

namespace content {

namespace {

std::atomic<uint32_t> g_salt(0);

}  // namespace

uint32_t GetPseudonymizationSalt() {
  uint32_t salt = g_salt.load();

  DCHECK(salt);

  return salt;
}

void SetPseudonymizationSalt(uint32_t salt) {
  DCHECK_NE(0u, salt);

#if DCHECK_IS_ON()
  uint32_t old_salt = g_salt.load(std::memory_order_acquire);
  // Permit the same salt to be set more than once. This is because for single
  // process tests and certain specific tests (e.g.
  // RenderThreadImplBrowserTest), the ChildProcess is running in the same
  // memory space as the browser.
  DCHECK(0 == old_salt || old_salt == salt);
#endif  // DCHECK_IS_ON()

  g_salt.store(salt);
}

void ResetSaltForTesting() {
  g_salt.store(0);
}

}  // namespace content
