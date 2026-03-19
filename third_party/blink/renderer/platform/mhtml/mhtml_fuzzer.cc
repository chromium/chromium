// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/testing/blink_fuzzer_test_support.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// Fuzzer for blink::MHTMLParser.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static BlinkFuzzerTestSupport test_support = BlinkFuzzerTestSupport();
  blink::test::TaskEnvironment task_environment;

  MHTMLParser mhtml_parser(SharedBuffer::Create(data));
  HeapVector<Member<ArchiveResource>> mhtml_archives =
      mhtml_parser.ParseArchive();
  mhtml_archives.clear();
  ThreadState::Current()->CollectAllGarbageForTesting();

  return 0;
}

}  // namespace blink

