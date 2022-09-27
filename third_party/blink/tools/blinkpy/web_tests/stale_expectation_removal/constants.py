# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants for stale expectation removal."""

import os

WEB_TEST_ROOT_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..',
                 'web_tests'))
