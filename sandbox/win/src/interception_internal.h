// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines InterceptionManager, the class in charge of setting up interceptions
// for the sandboxed process. For more details see:
// http://dev.chromium.org/developers/design-documents/sandbox .

#ifndef SANDBOX_SRC_INTERCEPTION_INTERNAL_H_
#define SANDBOX_SRC_INTERCEPTION_INTERNAL_H_

#include <stddef.h>

#include "sandbox/win/src/interceptors.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

const int kMaxThunkDataBytes = 64;

// The following structures contain variable size fields at the end, and will be
// used to transfer information between two processes. In order to guarantee
// our ability to follow the chain of structures, the alignment should be fixed,
// hence this pragma.
#pragma pack(push, 4)

// Structures for the shared memory that contains patching information
// for the InterceptionAgent.
// A single interception:
struct FunctionInfo {
  size_t record_bytes;  // rounded to sizeof(size_t) bytes
  InterceptionType type;
  InterceptorId id;
  const void* interceptor_address;
  char function[1];  // placeholder for null terminated name
  // char interceptor[]           // followed by the interceptor function
};

// A single dll:
struct DllPatchInfo {
  size_t record_bytes;  // rounded to sizeof(size_t) bytes
  size_t offset_to_functions;
  int num_functions;
  bool unload_module;
  wchar_t dll_name[1];  // placeholder for null terminated name
  // FunctionInfo function_info[] // followed by the functions to intercept
};

// All interceptions:
struct SharedMemory {
  int num_intercepted_dlls;
  void* interceptor_base;
  DllPatchInfo dll_list[1];  // placeholder for the list of dlls
};

// Dummy single thunk:
struct ThunkData {
  char data[kMaxThunkDataBytes];
};

// In-memory representation of the interceptions for a given dll:
struct DllInterceptionData {
  size_t data_bytes;
  size_t used_bytes;
  void* base;
  int num_thunks;
#if defined(_WIN64)
  int dummy;  // Improve alignment.
#endif
  ThunkData thunks[1];
};

#pragma pack(pop)

}  // namespace sandbox

#endif  // SANDBOX_SRC_INTERCEPTION_INTERNAL_H_
