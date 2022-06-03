#!/usr/bin/env vpython
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Triggers and processes results from flag try jobs.

For more information, see: http://bit.ly/flag-try-jobs
"""

import sys

from blinkpy.web_tests import try_flag

if __name__ == '__main__':
    sys.exit(try_flag.main())
