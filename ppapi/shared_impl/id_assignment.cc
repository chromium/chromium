// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/id_assignment.h"

#include <stdint.h>

namespace ppapi {

const unsigned int kPPIdTypeBits = 2;

const int32_t kMaxPPId = INT32_MAX >> kPPIdTypeBits;

static_assert(PP_ID_TYPE_COUNT <= (1 << kPPIdTypeBits),
              "kPPIdTypeBits is too small for all id types");

}  // namespace ppapi
