// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"

#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/test/test_timeouts.h"
#include "content/public/test/blink_test_environment.h"
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

  content::SetUpBlinkTestEnvironment();
}

BlinkFuzzerTestSupport::~BlinkFuzzerTestSupport() {
#if defined(ADDRESS_SANITIZER)
  // LSAN needs unreachable objects to be released to avoid reporting them
  // incorrectly as a memory leak.
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
#endif  // defined(ADDRESS_SANITIZER)
}

v8::Isolate* BlinkFuzzerTestSupport::GetIsolate() {
  return content::GetMainThreadIsolateForTestEnvironment();
}

}  // namespace blink
