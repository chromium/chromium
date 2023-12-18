#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.web_test_analyzers import fuzzy_diff_analyzer

import sys

if __name__ == '__main__':
    rc = fuzzy_diff_analyzer.main()
    sys.exit(rc)
