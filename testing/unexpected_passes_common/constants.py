# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Constants for unexpected pass finders."""

import os

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..'))
SRC_INTERNAL_DIR = os.path.realpath(
    os.path.join(CHROMIUM_SRC_DIR, '..', 'src-internal'))


# pylint: disable=useless-object-inheritance
class BuilderTypes(object):
  CI = 'ci'
  TRY = 'try'
