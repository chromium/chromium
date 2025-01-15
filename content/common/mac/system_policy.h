// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_SYSTEM_POLICY_H_
#define CONTENT_COMMON_MAC_SYSTEM_POLICY_H_

#include <stdint.h>

#include "base/types/expected.h"

namespace content {

struct MachTaskPortPolicy {
  // The configuration status value of the MACF policy system call.
  uint64_t amfi_status = 0;

  // Returns true if `amfi_status` indicates that the "allow everything" bit is
  // set, which corresponds to the `amfi_get_out_of_my_way` kernel boot
  // argument.
  bool AmfiIsAllowEverything() const;
};

// Gets the current MachTaskPortPolicy.
// Returns `errno` if an error occurred.
base::expected<MachTaskPortPolicy, int> GetMachTaskPortPolicy();

// Set crash keys containing system policy state for the lifetime of the
// process to help debug failures.
void SetSystemPolicyCrashKeys();

}  // namespace content

#endif  // CONTENT_COMMON_MAC_SYSTEM_POLICY_H_
