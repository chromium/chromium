# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Blink impl of //build/skia_gold_common/skia_gold_session_manager.py"""

from skia_gold_common import output_managerless_skia_gold_session as omsgs
from skia_gold_common import skia_gold_session_manager as sgsm


class BlinkSkiaGoldSessionManager(sgsm.SkiaGoldSessionManager):
    @staticmethod
    def GetSessionClass():
        return omsgs.OutputManagerlessSkiaGoldSession