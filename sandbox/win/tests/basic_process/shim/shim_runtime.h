// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_SHIM_RUNTIME_H_
#define SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_SHIM_RUNTIME_H_

#include <windows.h>
#include <winternl.h>

#include "sandbox/win/tests/basic_process/nocrt.h"
#include "sandbox/win/tests/basic_process/shim/logging.h"

// Shared runtime glue for the apifw.dll stub thunks: the marker macros, the
// lazily-resolving ORIGINAL_FN machinery, and the globals/helpers the thunks
// reach back into.
//
// apifw.dll's exports come solely from exports_logging.def, so the stub
// definitions carry no __declspec(dllexport). This marker documents exported
// stub definitions without affecting code generation.
#define BASIC_STUB_EXPORT

extern "C" {

// This DLL's module handle, captured during DLL_PROCESS_ATTACH.
extern HMODULE g_module;

// Resolves a module handle, loading the DLL on demand if it is not already
// present (e.g. winmm.dll, whose imports were redirected to apifw.dll).
HMODULE WINAPI GetOrLoadModule(LPCWSTR module_name);

// Resolves a bare DLL name against the parent of the EXE directory.
bool ResolveInParentDir(const wchar_t* dll_name, wchar_t* buf, DWORD buf_len);

}  // extern "C"

// Forwarding thunks use this pattern:
//   ORIGINAL_FN_DECL(ApiName);
//   ... ORIGINAL_FN_RESOLVE(ApiName, L"module.dll");
//   ... ORIGINAL_FN(ApiName)(args);
//
// ORIGINAL_FN_RESOLVE resolves and caches the real export once; it also logs
// the thunk unless the _NOLOG form is used. ORIGINAL_FN applies the audit gate
// before returning the cached function pointer.
#define ORIGINAL_FN_DECL(name)                         \
  [[maybe_unused]] static decltype(&name) name##_func; \
  [[maybe_unused]] static INIT_ONCE name##_Init = INIT_ONCE_STATIC_INIT

template <typename Fn>
inline void ApifwResolveOriginalFn(INIT_ONCE* init,
                                   Fn* slot,
                                   LPCWSTR mod,
                                   const char* name) {
  struct Ctx {
    Fn* slot;
    LPCWSTR mod;
    const char* name;
  } ctx{slot, mod, name};
  InitOnceExecuteOnce(
      init,
      [](PINIT_ONCE, PVOID parameter, PVOID*) -> BOOL {
        auto* c = static_cast<Ctx*>(parameter);
        *c->slot = reinterpret_cast<Fn>(
            GetProcAddress(GetOrLoadModule(c->mod), c->name));
        return TRUE;
      },
      &ctx, nullptr);
}

#define ORIGINAL_FN_RESOLVE_NOLOG(name, mod) \
  ApifwResolveOriginalFn(&name##_Init, &name##_func, mod, #name)
#define ORIGINAL_FN_RESOLVE(name, mod)    \
  do {                                    \
    LOG();                                \
    ORIGINAL_FN_RESOLVE_NOLOG(name, mod); \
  } while (0)
#define ORIGINAL_FN_RESOLVE_WITH_ARG(name, mod, arg) \
  do {                                               \
    LOG1(arg);                                       \
    ORIGINAL_FN_RESOLVE_NOLOG(name, mod);            \
  } while (0)
inline void ApifwAuditForwardGate() {
  if (!g_audit_basic_sandbox) {
    __debugbreak();
    __builtin_unreachable();
  }
}
#define ORIGINAL_FN(name) (ApifwAuditForwardGate(), name##_func)

#define APIFW_INTERNAL_CONCAT_IMPL(a, b) a##b
#define APIFW_INTERNAL_CONCAT(a, b) APIFW_INTERNAL_CONCAT_IMPL(a, b)
#define APIFW_INTERNAL_PARAMETER_COUNT_IMPL(_1, _2, _3, _4, _5, _6, _7, _8,   \
                                            _9, _10, _11, _12, _13, _14, _15, \
                                            _16, count, ...)                  \
  count
#define APIFW_INTERNAL_PARAMETER_COUNT(...)                                    \
  APIFW_INTERNAL_PARAMETER_COUNT_IMPL(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, \
                                      9, 8, 7, 6, 5, 4, 3, 2, 1)

#define APIFW_INTERNAL_PARAMETER_DECL(parameter) \
  APIFW_INTERNAL_PARAMETER_DECL_IMPL parameter
#define APIFW_INTERNAL_PARAMETER_DECL_IMPL(type, name) type name
#define APIFW_INTERNAL_PARAMETER_NAME(parameter) \
  APIFW_INTERNAL_PARAMETER_NAME_IMPL parameter
#define APIFW_INTERNAL_PARAMETER_NAME_IMPL(type, name) name

#define APIFW_INTERNAL_MAP_1(macro, p1) macro(p1)
#define APIFW_INTERNAL_MAP_2(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_1(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_3(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_2(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_4(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_3(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_5(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_4(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_6(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_5(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_7(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_6(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_8(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_7(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_9(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_8(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_10(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_9(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_11(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_10(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_12(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_11(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_13(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_12(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_14(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_13(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_15(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_14(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_16(macro, p1, ...) \
  macro(p1), APIFW_INTERNAL_MAP_15(macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP_NONEMPTY(macro, ...)                      \
  APIFW_INTERNAL_CONCAT(APIFW_INTERNAL_MAP_,                         \
                        APIFW_INTERNAL_PARAMETER_COUNT(__VA_ARGS__)) \
  (macro, __VA_ARGS__)
#define APIFW_INTERNAL_MAP(macro, ...) \
  __VA_OPT__(APIFW_INTERNAL_MAP_NONEMPTY(macro, __VA_ARGS__))

// Parameters are (type, name) pairs so the declaration and forwarded argument
// list come from one source. Types with commas require a typedef; the mapper
// supports up to sixteen parameters.
#define APIFW_FORWARD_THUNK(name, module, ...)                               \
  ORIGINAL_FN_DECL(name);                                                    \
  BASIC_STUB_EXPORT auto WINAPI Apifw##name(                                 \
      APIFW_INTERNAL_MAP(APIFW_INTERNAL_PARAMETER_DECL, __VA_ARGS__))        \
      -> decltype(name(                                                      \
          APIFW_INTERNAL_MAP(APIFW_INTERNAL_PARAMETER_NAME, __VA_ARGS__))) { \
    ORIGINAL_FN_RESOLVE(name, module);                                       \
    return ORIGINAL_FN(name)(                                                \
        APIFW_INTERNAL_MAP(APIFW_INTERNAL_PARAMETER_NAME, __VA_ARGS__));     \
  }

#define APIFW_FORWARD_KERNEL32(name, ...) \
  APIFW_FORWARD_THUNK(name, L"kernel32.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_ADVAPI32(name, ...) \
  APIFW_FORWARD_THUNK(name, L"advapi32.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_BCRYPTPRIMITIVES(name, ...) \
  APIFW_FORWARD_THUNK(name, L"bcryptprimitives.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_COMBASE(name, ...) \
  APIFW_FORWARD_THUNK(name, L"combase.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_DWRITE(name, ...) \
  APIFW_FORWARD_THUNK(name, L"dwrite.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_DWRITECORE(name, ...) \
  APIFW_FORWARD_THUNK(name, L"DWriteCore.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_NTDLL(name, ...) \
  APIFW_FORWARD_THUNK(name, L"ntdll.dll" __VA_OPT__(, ) __VA_ARGS__)

#define APIFW_FORWARD_WINMM(name, ...) \
  APIFW_FORWARD_THUNK(name, L"winmm.dll" __VA_OPT__(, ) __VA_ARGS__)

#endif  // SANDBOX_WIN_TESTS_BASIC_PROCESS_SHIM_SHIM_RUNTIME_H_
