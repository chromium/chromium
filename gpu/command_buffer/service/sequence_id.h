// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SEQUENCE_ID_H_
#define GPU_COMMAND_BUFFER_SERVICE_SEQUENCE_ID_H_

#include "base/util/type_safety/id_type.h"

namespace gpu {

class SyncPointOrderData;
using SequenceId = util::IdTypeU32<SyncPointOrderData>;

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SEQUENCE_ID_H_
