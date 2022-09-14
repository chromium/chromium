// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_unittest_util.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "media/base/media_switches.h"

namespace media {

// For macro ABORT_AUDIO_TEST_IF_NOT.
bool ShouldAbortAudioTest(bool requirements_satisfied,
                          const char* requirements_expression,
                          bool* should_fail) {
  bool fail_if_unsatisfied = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kRequireAudioHardwareForTesting);
  if (!requirements_satisfied) {
    LOG(WARNING) << "Requirement(s) not satisfied (" << requirements_expression
                 << ")";
    *should_fail = fail_if_unsatisfied;
    return true;
  }
  *should_fail = false;
  return false;
}

}  // namespace media
