# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

CHROMIUM_SRC_DIR = os.path.join(os.path.dirname(__file__), '..', '..')
TYP_PATH = os.path.join(CHROMIUM_SRC_DIR, 'third_party', 'catapult',
                        'third_party', 'typ')

if TYP_PATH not in sys.path:
  sys.path.append(TYP_PATH)
