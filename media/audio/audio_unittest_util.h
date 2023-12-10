// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_

// Use in tests to skip a test when the system is missing a required audio
// device or library.
#define ABORT_AUDIO_TEST_IF_NOT(requirements_satisfied) \
  do {                                                  \
    if (!(requirements_satisfied)) {                    \
      return;                                           \
    }                                                   \
  } while (false)

#endif  // MEDIA_AUDIO_AUDIO_UNITTEST_UTIL_H_
