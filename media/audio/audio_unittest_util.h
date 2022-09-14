// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Use in tests to either skip or fail a test when the system is missing a
// required audio device or library. If the --require-audio-hardware-for-testing
// flag is set, missing requirements will cause the test to fail. Otherwise it
// will be skipped.
#define ABORT_AUDIO_TEST_IF_NOT(requirements_satisfied)                       \
  do {                                                                        \
    bool fail = false;                                                        \
    if (ShouldAbortAudioTest(requirements_satisfied, #requirements_satisfied, \
                             &fail)) {                                        \
      if (fail)                                                               \
        FAIL();                                                               \
      else                                                                    \
        return;                                                               \
    }                                                                         \
  } while (false)

bool ShouldAbortAudioTest(bool requirements_satisfied,
                          const char* requirements_expression,
                          bool* should_fail);

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_
