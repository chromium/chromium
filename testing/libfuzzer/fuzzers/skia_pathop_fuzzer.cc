// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
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

#include "base/containers/span_reader.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> bytes) {
  base::SpanReader reader(bytes);
  SkOpBuilder builder;
  for (uint8_t op; read(reader, &op);) {
    const SkPath path = BuildPath(reader, SkPath::Verb::kDone_Verb);
    builder.add(path, static_cast<SkPathOp>(op % (kLastOp + 1)));
  }

  SkPath result;
  builder.resolve(&result);
  return 0;
}
