# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..',
                 '..'))
TESTING_DIR = os.path.join(CHROMIUM_SRC_DIR, 'testing')

if TESTING_DIR not in sys.path:
    sys.path.append(TESTING_DIR)
