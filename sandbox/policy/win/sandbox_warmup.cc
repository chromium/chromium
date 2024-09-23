// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include <windows.h>

#include "base/check_op.h"

// Prototype for ProcessPrng.
// See: https://learn.microsoft.com/en-us/windows/win32/seccng/processprng
extern "C" {
BOOL WINAPI ProcessPrng(PBYTE pbData, SIZE_T cbData);
}

namespace sandbox::policy {

namespace {

// Import bcryptprimitives!ProcessPrng rather than cryptbase!RtlGenRandom to
// avoid opening a handle to \\Device\KsecDD in the renderer.
decltype(&ProcessPrng) GetProcessPrng() {
  HMODULE hmod = LoadLibraryW(L"bcryptprimitives.dll");
  CHECK(hmod);
  decltype(&ProcessPrng) process_prng_fn =
      reinterpret_cast<decltype(&ProcessPrng)>(
          GetProcAddress(hmod, "ProcessPrng"));
  CHECK(process_prng_fn);
  return process_prng_fn;
}

}  // namespace

void WarmupRandomnessInfrastructure() {
  BYTE data[1];
  // TODO(crbug.com/40088338) Call a warmup function exposed by boringssl.
  static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
  BOOL success = process_prng_fn(data, sizeof(data));
  // ProcessPrng is documented to always return TRUE.
  CHECK(success);
}

}  // namespace sandbox::policy
