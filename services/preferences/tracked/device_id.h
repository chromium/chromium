// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_PREFERENCES_TRACKED_DEVICE_ID_H_
#define SERVICES_PREFERENCES_TRACKED_DEVICE_ID_H_

#include <string>

enum class MachineIdStatus {
  SUCCESS = 0,
  FAILURE,         // Returned if attempt to obtain a machine-specific ID fails.
  NOT_IMPLEMENTED  // Returned if the method for obtaining a machine-specific ID
                   // is not implemented for the system.
};

// Populates |machine_id| with a deterministic ID for this machine. |machine_id|
// must not be null. Returns |FAILURE| if a machine ID cannot be obtained or
// |NOT_IMPLEMENTED| on systems for which this feature is not supported (in both
// cases |machine_id| is left untouched).
MachineIdStatus GetDeterministicMachineSpecificId(std::string* machine_id);

#endif  // SERVICES_PREFERENCES_TRACKED_DEVICE_ID_H_
