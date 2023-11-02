// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "services/preferences/tracked/device_id.h"

MachineIdStatus GetDeterministicMachineSpecificId(std::string* machine_id) {
  DCHECK(machine_id);
  return MachineIdStatus::NOT_IMPLEMENTED;
}
