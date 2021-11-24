#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Run web_tests (aka LayoutTests)"""

from blinkpy.common import multiprocessing_bootstrap

multiprocessing_bootstrap.run('blinkpy', 'web_tests', 'run_web_tests.py')
