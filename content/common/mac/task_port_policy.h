// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_
#define CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_

#include <cstdint>

namespace content {

struct MachTaskPortPolicy {
  // Return value of undocumented MACF policy system call to AMFI to get the
  // configuration status.
  int amfi_status_retval = 0;
  // The configuration status value of the MACF policy system call.
  uint64_t amfi_status = 0;

  // Returns true if `amfi_status` indicates that the "allow everything" bit is
  // set, which corresponds to the `amfi_get_out_of_my_way` kernel boot
  // argument.
  bool AmfiIsAllowEverything() const;
};

// Gets the current MachTaskPortPolicy.
MachTaskPortPolicy GetMachTaskPortPolicy();

}  // namespace content

#endif  // CONTENT_COMMON_MAC_TASK_PORT_POLICY_H_
