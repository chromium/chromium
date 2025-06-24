# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants used in various modules."""

# Default Timeout (seconds) to kill a test process when it doesn't have output.
READLINE_TIMEOUT = 180

# Maximum number of days that we should keep stale iOS simulator runtimes
MAX_RUNTIME_KEPT_DAYS = '3'

# Maximum number of simulator runtime we should keep in any given time
MAX_RUNTIME_KEPT_COUNT = 3

# Message printed in EG test logs to determine if the app crashed
CRASH_MESSAGE = 'App crashed and disconnected.'

# Message printed in host app test logs when an ASan error occurs
ASAN_ERROR = 'ERROR: AddressSanitizer'

LAYOUT_CONSTRAINT_MSG = 'Omitting layout constraint warnings'
