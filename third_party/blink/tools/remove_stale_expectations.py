#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.stale_expectation_removal import (
    remove_stale_expectations)

import sys

if __name__ == '__main__':
    rc = remove_stale_expectations.main()
    sys.exit(rc)
