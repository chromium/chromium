// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_
#define CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_

#include <cstdint>
#include <string>

#include "base/strings/string_piece.h"
#include "content/common/content_export.h"

namespace content {

struct MachTaskPortPolicy {
  // Return value of undocumented MACF policy system call to AMFI to get the
  // configuration status.
  int amfi_status_retval = 0;
  // The configuration status value of the MACF policy system call.
  uint64_t amfi_status = 0;

  // Filtered kernel boot arguments relevant to AMFI and task control port
  // policy.
  std::string boot_args{"unknown"};

  // Return value and errno of a csr_check() for allowing kernel debugging.
  int csr_kernel_debugger_retval = 0;
  int csr_kernel_debugger_errno = 0;
};

// Gets the current MachTaskPortPolicy.
MachTaskPortPolicy GetMachTaskPortPolicy();

// Filters the full kern.bootargs and extracts the relevant ones for
// MachTaskPortPolicy. Exposed for testing.
std::string CONTENT_EXPORT ParseBootArgs(base::StringPiece input);

}  // namespace content

#endif  // CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_
