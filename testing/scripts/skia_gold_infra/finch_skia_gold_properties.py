# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Finch implementation of skia_gold_properties.py."""

import os
import subprocess
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.realpath(os.path.join(THIS_DIR, '..', '..', '..'))
sys.path.insert(0, os.path.join(CHROMIUM_SRC_DIR, 'build'))
from skia_gold_common import skia_gold_properties


class FinchSkiaGoldProperties(skia_gold_properties.SkiaGoldProperties):
  @staticmethod
  def _GetGitOriginMainHeadSha1():
    try:
      return subprocess.check_output(
          ['git', 'rev-parse', 'origin/main'],
          shell=_IsWin(),
          cwd=CHROMIUM_SRC_DIR).strip()
    except subprocess.CalledProcessError:
      return None


def _IsWin():
  return sys.platform == 'win32'
