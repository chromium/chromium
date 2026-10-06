#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Rolls //third_party/android_deps/autorolled; thin wrapper for fetch_all.py.

All arguments are passed through to fetch_all.py (see its --help).
"""

import os
import pathlib
import sys

_AUTOROLLED_PATH = pathlib.Path(__file__).resolve().parent
_FETCH_ALL_PATH = _AUTOROLLED_PATH.parent / 'fetch_all.py'

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if a != '--']
    os.execv(sys.executable, [
        sys.executable,
        str(_FETCH_ALL_PATH), '--android-deps-dir',
        str(_AUTOROLLED_PATH)
    ] + args)
