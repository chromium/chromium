# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finch impl of skia_gold_session_manager.py."""

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(THIS_DIR, '..', '..', '..'))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'build'))
from skia_gold_common import output_managerless_skia_gold_session
from skia_gold_common import skia_gold_session_manager as sgsm


class FinchSkiaGoldSessionManager(sgsm.SkiaGoldSessionManager):
  @staticmethod
  def GetSessionClass():
    return output_managerless_skia_gold_session.OutputManagerlessSkiaGoldSession
