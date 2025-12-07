# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMIUM_SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..', '..',
                                '..')
BLINK_TOOLS_PATH = os.path.join(CHROMIUM_SRC_DIR, 'third_party', 'blink',
                                'tools')

if BLINK_TOOLS_PATH not in sys.path:
  sys.path.append(BLINK_TOOLS_PATH)
