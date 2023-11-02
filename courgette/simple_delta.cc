// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of the byte-level differential compression used internally by
// Courgette.

#include "courgette/simple_delta.h"

#include "base/logging.h"

#include "courgette/third_party/bsdiff/bsdiff.h"

namespace courgette {

namespace {

Status BSDiffStatusToStatus(bsdiff::BSDiffStatus status) {
  switch (status) {
    case bsdiff::OK: return C_OK;
    case bsdiff::CRC_ERROR: return C_BINARY_DIFF_CRC_ERROR;
    default: return C_GENERAL_ERROR;
  }
}

}

Status ApplySimpleDelta(SourceStream* old, SourceStream* delta,
                        SinkStream* target) {
  return BSDiffStatusToStatus(bsdiff::ApplyBinaryPatch(old, delta, target));
}

Status GenerateSimpleDelta(SourceStream* old, SourceStream* target,
                           SinkStream* delta) {
  VLOG(1) << "GenerateSimpleDelta " << old->Remaining()
          << " " << target->Remaining();
  return BSDiffStatusToStatus(bsdiff::CreateBinaryPatch(old, target, delta));
}

}  // namespace courgette
