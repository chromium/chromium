// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/test/test_timeouts.h"
#include "content/public/test/blink_test_environment.h"
#include "content/public/test/setup_field_trials.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
namespace blink {

BlinkFuzzerTestSupport::BlinkFuzzerTestSupport()
    : BlinkFuzzerTestSupport(0, nullptr) {}

BlinkFuzzerTestSupport::BlinkFuzzerTestSupport(int argc, char** argv) {
  // Note: we don't tear anything down here after an iteration of the fuzzer
  // is complete, this is for efficiency. We rerun the fuzzer with the same
  // environment as the previous iteration.
  CHECK(base::i18n::InitializeICU());

  base::CommandLine::Init(argc, argv);

  TestTimeouts::Initialize();

  // In unbranded builds, enable the features listed in
  // testing/variations/fieldtrial_testing_config.json
  // This allows fuzzing of features shipping to Beta/Dev/Stable users via a
  // field trial experiment.
  content::SetupFieldTrials();

  test_environment_ = std::make_unique<content::BlinkTestEnvironment>();
  test_environment_->SetUp();
}

BlinkFuzzerTestSupport::~BlinkFuzzerTestSupport() {
  test_environment_->TearDown();
}

}  // namespace blink
