// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/csspaint/paint_worklet_id_generator.h"

#include <limits>

#include "base/check_op.h"

namespace blink {

namespace {
// Note that the NextId() is called from the main thread only, and that's why
// it is fine with current_id being int. In the future, if NextId is called from
// a thread other than the main thread, then we should use AtomicSequenceNumber.
static int current_id = 0;
}  // namespace

int PaintWorkletIdGenerator::NextId() {
  CHECK_LT(current_id, std::numeric_limits<int>::max());
  return ++current_id;
}

}  // namespace blink
