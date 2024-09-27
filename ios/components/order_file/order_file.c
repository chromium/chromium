// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/order_file/order_file_common.h"

#import <stdlib.h>

// Queue containing the ordered procedure calls.
OSQueueHead gCRWSanitizerQueue = OS_ATOMIC_QUEUE_INIT;

// Whether the guard variables have been initialized.
bool gCRWGuardsInitialized = false;

// Whether the addition of more procedure calls is allowed.
bool gCRWFinishedCollecting = false;

void __sanitizer_cov_trace_pc_guard_init(uint32_t* start,
                                         uint32_t* stop);  // NOLINT
void __sanitizer_cov_trace_pc_guard(uint32_t* guard);      // NOLINT

// Initialize the guards for each procedure call.
// Per clang documentation, this function will be called at least once per
// Dynamic Shared Object (DSO) and may be called more than once with the same
// values of start/stop.
void __sanitizer_cov_trace_pc_guard_init(uint32_t* start_guard,
                                         uint32_t* stop_guard) {  // NOLINT
  gCRWGuardsInitialized = true;
  if (start_guard == stop_guard || *start_guard) {
    return;
  }
  for (uint32_t* guard = start_guard; guard < stop_guard; guard++) {
    // Per https://clang.llvm.org/docs/SanitizerCoverage.html, *guard is
    // initialized to 1 and set to 0 below.
    *guard = 1;
  }
}

// Collect each procedure call in order once for each distinct call.
// Per clang documentation, this callback is inserted by the compiler on every
// edge in the control flow (some optimizations apply).
void __sanitizer_cov_trace_pc_guard(uint32_t* guard) {  // NOLINT
  // If gGuardsInitialized is false, guard flags have not yet been initialized
  // and this is probably recording a +load method which  should still be
  // included in the order file.
  if (gCRWFinishedCollecting || (!(*guard) && gCRWGuardsInitialized)) {
    return;
  }
  // Make sure each procedure call is only collected once.
  *guard = 0;

  void* procedureCall = __builtin_return_address(0);
  CRWProcedureCallNode* node =
      (CRWProcedureCallNode*)malloc(sizeof(CRWProcedureCallNode));
  *node = (CRWProcedureCallNode){procedureCall, NULL};
  OSAtomicEnqueue(&gCRWSanitizerQueue, node,
                  offsetof(CRWProcedureCallNode, next));
}
