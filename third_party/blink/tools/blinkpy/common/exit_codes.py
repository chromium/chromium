# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import signal

OK_EXIT_STATUS = 0

# This matches what the shell does on POSIX (returning -SIGNUM on unhandled
# signal). (unsigned)(-SIGINT) == 128+signal.SIGINT
INTERRUPTED_EXIT_STATUS = signal.SIGINT + 128

# POSIX limits status codes to 0-255. Normally run_web_tests.py returns the
# number of tests that failed. These indicate exceptional conditions triggered
# by the script itself, so we count backwards from 255 (aka -1) to enumerate
# them.
#
# FIXME: crbug.com/357866. We really shouldn't return the number of failures
# in the exit code at all.
EARLY_EXIT_STATUS = 251
SYS_DEPS_EXIT_STATUS = 252
NO_TESTS_EXIT_STATUS = 253
NO_DEVICES_EXIT_STATUS = 254
UNEXPECTED_ERROR_EXIT_STATUS = 255

# FIXME: EXCEPTIONAL_EXIT_STATUS and NO_DEVICES_EXIT_STATUS conflict
EXCEPTIONAL_EXIT_STATUS = 254

ERROR_CODES = (
    INTERRUPTED_EXIT_STATUS,
    EARLY_EXIT_STATUS,
    SYS_DEPS_EXIT_STATUS,
    NO_TESTS_EXIT_STATUS,
    NO_DEVICES_EXIT_STATUS,
    UNEXPECTED_ERROR_EXIT_STATUS,
)

# In order to avoid colliding with the above codes, we put a ceiling on
# the value returned by num_regressions
MAX_FAILURES_EXIT_STATUS = 101
