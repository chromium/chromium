// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/logging.h"
#include "testing/libfuzzer/fuzzers/skia_path_common.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/pathops/SkPathOps.h"

struct Environment {
  Environment() {
    // Disable noisy logging as per "libFuzzer in Chrome" documentation:
    // testing/libfuzzer/getting_started.md#Disable-noisy-error-message-logging.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }
};

Environment* env = new Environment();

const int kLastOp = SkPathOp::kReverseDifference_SkPathOp;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  SkOpBuilder builder;
  while (size > 0) {
    SkPath path;
    uint8_t op;
    if (!read<uint8_t>(&data, &size, &op))
      break;

    BuildPath(&data, &size, &path, SkPath::Verb::kDone_Verb);
    builder.add(path, static_cast<SkPathOp>(op % (kLastOp + 1)));
  }

  SkPath result;
  builder.resolve(&result);
  return 0;
}
