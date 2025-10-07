// Copyright 2006-2008 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines InterceptionAgent, the class in charge of setting up interceptions
// from the inside of the sandboxed process. For more details see
// http://dev.chromium.org/developers/design-documents/sandbox .

#ifndef SANDBOX_WIN_SRC_INTERCEPTION_AGENT_H_
#define SANDBOX_WIN_SRC_INTERCEPTION_AGENT_H_

#include <windows.h>

#include <winternl.h>

#include "base/memory/raw_ptr_exclusion.h"
#include "sandbox/win/src/sandbox_types.h"

namespace sandbox {

// Internal structures used for communication between the broker and the target.
struct DllInterceptionData;
struct SharedMemory;
struct DllPatchInfo;

class ResolverThunk;

// The InterceptionAgent executes on the target application, and it is in charge
// of setting up the desired interceptions or indicating what module needs to
// be unloaded.
//
// The exposed API consists of three methods: GetInterceptionAgent to retrieve
// the single class instance, OnDllLoad and OnDllUnload to process a dll being
// loaded and unloaded respectively.
//
// This class assumes that it will get called for every dll being loaded,
// starting with kernel32, so the singleton will be instantiated from within the
// loader lock.
class InterceptionAgent {
 public:
  InterceptionAgent() = delete;

  // Returns the single InterceptionAgent object for this process.
  static InterceptionAgent* GetInterceptionAgent();

  InterceptionAgent(const InterceptionAgent&) = delete;
  InterceptionAgent& operator=(const InterceptionAgent&) = delete;

  // This method should be invoked whenever a new dll is loaded to perform the
  // required patches. If the return value is false, this dll should not be
  // allowed to load.
  //
  // full_path is the (optional) full name of the module being loaded and name
  // is the internal module name. If full_path is provided, it will be used
  // before the internal name to determine if we care about this dll.
  bool OnDllLoad(const UNICODE_STRING* full_path, const UNICODE_STRING* name,
                 void* base_address);

  // Performs cleanup when a dll is unloaded.
  void OnDllUnload(void* base_address);

 private:
  ~InterceptionAgent() {}

  // Performs initialization of the singleton.
  bool Init(SharedMemory* shared_memory);

  // Returns true if we are interested on this dll. dll_info is an entry of the
  // list of intercepted dlls.
  bool DllMatch(const UNICODE_STRING* full_path, const UNICODE_STRING* name,
                const DllPatchInfo* dll_info);

  // Performs the patching of the dll loaded at base_address.
  // The patches to perform are described on dll_info, and thunks is the thunk
  // storage for the whole dll.
  // Returns true on success.
  bool PatchDll(const DllPatchInfo* dll_info, DllInterceptionData* thunks);

  // Returns a resolver for a given interception type.
  ResolverThunk* GetResolver(InterceptionType type);

  // Shared memory containing the list of functions to intercept.
  // RAW_PTR_EXCLUSION: Accessed too early during the process startup to support
  // raw_ptr<T>.
  RAW_PTR_EXCLUSION SharedMemory* interceptions_;

  // Array of thunk data buffers for the intercepted dlls. This object singleton
  // is allocated with a placement new with enough space to hold the complete
  // array of pointers, not just the first element.
  DllInterceptionData* dlls_[1];
};

}  // namespace sandbox

#endif  // SANDBOX_WIN_SRC_INTERCEPTION_AGENT_H_
