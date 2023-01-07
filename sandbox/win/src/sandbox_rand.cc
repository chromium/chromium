// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/sandbox_rand.h"

#include <windows.h>

// #define needed to link in RtlGenRandom(), a.k.a. SystemFunction036.  See the
// "Community Additions" comment on MSDN here:
// http://msdn.microsoft.com/en-us/library/windows/desktop/aa387694.aspx
#define SystemFunction036 NTAPI SystemFunction036
#include <NTSecAPI.h>
#undef SystemFunction036

namespace sandbox {

bool GetRandom(unsigned int* random_value) {
  return RtlGenRandom(random_value, sizeof(unsigned int)) != false;
}

}  // namespace sandbox
