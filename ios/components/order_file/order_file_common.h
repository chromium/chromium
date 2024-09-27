// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_ORDER_FILE_ORDER_FILE_COMMON_H_
#define IOS_COMPONENTS_ORDER_FILE_ORDER_FILE_COMMON_H_

#import <libkern/OSAtomicQueue.h>

#include "base/memory/raw_ptr_exclusion.h"

#ifdef __cplusplus
extern "C" {
#endif

// A struct representing a procedure call.
typedef struct {
  RAW_PTR_EXCLUSION void* procedureCall;
  RAW_PTR_EXCLUSION void* next;  // Used for offset.
} CRWProcedureCallNode;

// Queue containing the ordered procedure calls.
extern OSQueueHead gCRWSanitizerQueue;

// Whether the guard variables have been initialized.
extern bool gCRWGuardsInitialized;

// Whether the addition of more procedure calls is allowed.
extern bool gCRWFinishedCollecting;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // IOS_COMPONENTS_ORDER_FILE_ORDER_FILE_COMMON_H_
