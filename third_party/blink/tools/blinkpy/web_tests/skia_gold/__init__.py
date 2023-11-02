# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

from blinkpy.common import path_finder

BUILD_DIR = os.path.join(path_finder.get_chromium_src_dir(), 'build')

if BUILD_DIR not in sys.path:
    sys.path.append(BUILD_DIR)
