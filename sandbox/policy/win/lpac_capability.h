// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_
#define SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_

#include "sandbox/policy/export.h"

namespace sandbox {
namespace policy {

SANDBOX_POLICY_EXPORT extern const wchar_t kMediaFoundationCdmData[];
SANDBOX_POLICY_EXPORT extern const wchar_t kMediaFoundationCdmFiles[];

}  // namespace policy
}  // namespace sandbox

#endif  // SANDBOX_POLICY_WIN_LPAC_CAPABILITY_H_
