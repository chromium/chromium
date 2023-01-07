// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Macros specific to the RLZ library.

#include "rlz/lib/assert.h"

namespace rlz_lib {

#ifdef MUTE_EXPECTED_ASSERTS
std::string expected_assertion_;
#endif

}  // namespace rlz_lib
