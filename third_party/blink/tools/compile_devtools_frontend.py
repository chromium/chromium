#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Perform type check for devtools frontend

This script wraps devtools-frontend/src/scripts/test/run_type_check.py
DevTools bot kicks this script.
"""

import os
import sys

sys.path.append(
    os.path.join(
        os.path.dirname(__file__), '..', '..', 'devtools-frontend', 'src',
        'scripts', 'test'))
import run_type_check

if __name__ == '__main__':
    sys.exit(run_type_check.main())
