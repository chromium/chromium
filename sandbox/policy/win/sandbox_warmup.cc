// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include "base/feature_list.h"
#include "sandbox/policy/features.h"

#include <windows.h>

// Note: do not copy this to add new uses of RtlGenRandom.
// Prefer: crypto::RandBytes, base::RandBytes or bcryptprimitives!ProcessPrng.
// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

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

  if (base::FeatureList::IsEnabled(
          sandbox::policy::features::kWinSboxWarmupProcessPrng)) {
    // TODO(crbug.com/74242) Call a warmup function exposed by boringssl.
    static decltype(&ProcessPrng) process_prng_fn = GetProcessPrng();
    BOOL success = process_prng_fn(data, sizeof(data));
    // ProcessPrng is documented to always return TRUE.
    CHECK(success);
  } else {
    // This loads advapi!SystemFunction036 which is forwarded to
    // cryptbase!SystemFunction036. This allows boringsll and Chrome to call
    // RtlGenRandom from within the sandbox. This has the unfortunate side
    // effect of opening a handle to \\Device\KsecDD which we will later close
    // in processes that do not need this.
    RtlGenRandom(data, sizeof(data));
  }
}

}  // namespace sandbox::policy
