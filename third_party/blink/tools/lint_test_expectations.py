#!/usr/bin/env vpython3
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from blinkpy.web_tests.lint_test_expectations import main

if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stderr))
