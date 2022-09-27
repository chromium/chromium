# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink implementation of //build/skia_gold_common/skia_gold_properties.py"""

import subprocess
import sys

from blinkpy.common import path_finder
from skia_gold_common import skia_gold_properties


class BlinkSkiaGoldProperties(skia_gold_properties.SkiaGoldProperties):
    @staticmethod
    def _GetGitOriginMainHeadSha1():
        try:
            return subprocess.check_output(
                ['git', 'rev-parse', 'origin/main'],
                shell=_IsWin(),
                cwd=path_finder.get_chromium_src_dir()).decode(
                    'utf-8').strip()
        except subprocess.CalledProcessError:
            return None


def _IsWin():
    return sys.platform == 'win32'
