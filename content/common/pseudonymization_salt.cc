// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/pseudonymization_salt.h"

#include <atomic>

#include "base/check_op.h"
#include "base/dcheck_is_on.h"
#include "base/rand_util.h"

#if DCHECK_IS_ON()
#include "sandbox/policy/sandbox.h"
#endif

namespace content {

namespace {

std::atomic<uint32_t> g_salt(0);

uint32_t InitializeSalt() {
  uint32_t salt;
  do {
    salt = base::RandUint64();
  } while (salt == 0);

  // If `g_salt` is still uninitialized (has a value of 0), then put `salt` into
  // `g_salt`.  Otherwise, use the current `value` of `g_salt`.
  uint32_t value = 0;
  if (!g_salt.compare_exchange_strong(value, salt))
    salt = value;

  return salt;
}

}  // namespace

uint32_t GetPseudonymizationSalt() {
  uint32_t salt = g_salt.load();

  if (salt == 0) {
#if DCHECK_IS_ON()
    // Only the Browser process needs to initialize the `salt` on demand.
    // Other processes (identified via the IsProcessSandboxed heuristic) should
    // receive the salt from their parent processes.
    DCHECK(!sandbox::policy::Sandbox::IsProcessSandboxed());
#endif
    salt = InitializeSalt();
  }

  return salt;
}

void SetPseudonymizationSalt(uint32_t salt) {
  DCHECK_NE(0u, salt);

  // TODO(lukasza): Ideally we would DCHECK that `g_salt` is not set twice (e.g.
  // that DCHECK_EQ(0u, g_salt.load(std::memory_order_acquire))), but this is
  // made rather difficult by tests that run in single-process-mode, or
  // construct ChildProcessHostImpl directly (e.g. RenderThreadImplBrowserTest).

  g_salt.store(salt);
}

}  // namespace content
