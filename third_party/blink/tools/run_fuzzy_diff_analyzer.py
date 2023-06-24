#!/usr/bin/env vpython3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from blinkpy.web_tests.fuzzy_diff_analyzer import (fuzzy_diff_analyzer)

import sys

if __name__ == '__main__':
    rc = fuzzy_diff_analyzer.main()
    sys.exit(rc)
