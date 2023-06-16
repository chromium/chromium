// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/policy/win/sandbox_warmup.h"

#include <windows.h>

// Note: do not copy this to add new uses of RtlGenRandom.
// Prefer: crypto::RandBytes, base::RandBytes or bcryptprimitives!ProcessPrng.
// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

namespace sandbox::policy {

void WarmupRandomnessInfrastructure() {
  // This loads advapi!SystemFunction036 which is forwarded to
  // cryptbase!SystemFunction036. This allows boringsll and Chrome to call
  // RtlGenRandom from within the sandbox. This has the unfortunate side effect
  // of opening a handle to \\Device\KsecDD which we will later close in
  // processes that do not need this. Ideally everyone would call ProcessPrng in
  // bcryptprimitives instead and this warmup can change to load that directly.
  // TODO(crbug.com/74242) swap boringssl to ProcessPrng from RtlGenRandom.
  // TODO(crbug.com/74242) swap Chrome to ProcessPrng from RtlGenRandom.
  char data[1];
  RtlGenRandom(data, sizeof(data));
}

}  // namespace sandbox::policy
