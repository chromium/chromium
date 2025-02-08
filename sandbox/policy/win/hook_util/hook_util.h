// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_HOOK_UTIL_HOOK_UTIL_H_
#define SANDBOX_POLICY_WIN_HOOK_UTIL_HOOK_UTIL_H_

#include <windows.h>

// TODO(crbug.com/394519481) remove once a clang roll happens.
// Short term fix to allow raw pointers until RawPtrManualPathsToIgnore updates.
// Cannot use raw_ptr_exclusion.h as chrome_elf cannot use //base.
#if !defined(TEMP_RAW_PTR_EXCLUSION)

#if defined(__has_attribute)
#define SANDBOX_IAT_HOOK_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#define SANDBOX_IAT_HOOK_HAS_ATTRIBUTE(x) 0
#endif

#if SANDBOX_IAT_HOOK_HAS_ATTRIBUTE(annotate)
#define TEMP_RAW_PTR_EXCLUSION __attribute__((annotate("raw_ptr_exclusion")))
#else
#define TEMP_RAW_PTR_EXCLUSION
#endif

#endif  // !defined(RAW_PTR_EXCLUSION)

namespace sandbox::policy {

//------------------------------------------------------------------------------
// Import Address Table hooking support
//------------------------------------------------------------------------------
class IATHook {
 public:
  IATHook();

  IATHook(const IATHook&) = delete;
  IATHook& operator=(const IATHook&) = delete;

  ~IATHook();

  // Intercept a function in an import table of a specific
  // module. Saves everything needed to Unhook.
  //
  // NOTE: Hook can only be called once at a time, to enable Unhook().
  //
  // Arguments:
  // module                 Module to be intercepted
  // imported_from_module   Module that exports the 'function_name'
  // function_name          Name of the API to be intercepted
  // new_function           New function pointer
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful.
  DWORD Hook(HMODULE module,
             const char* imported_from_module,
             const char* function_name,
             void* new_function);

  // Unhook the IAT entry.
  //
  // Returns: Windows error code (winerror.h). NO_ERROR if successful.
  DWORD Unhook();

 private:
  TEMP_RAW_PTR_EXCLUSION void* intercept_function_;
  TEMP_RAW_PTR_EXCLUSION void* original_function_;
  TEMP_RAW_PTR_EXCLUSION IMAGE_THUNK_DATA* iat_thunk_;
};

}  // namespace sandbox::policy

#endif  // SANDBOX_POLICY_WIN_HOOK_UTIL_HOOK_UTIL_H_
