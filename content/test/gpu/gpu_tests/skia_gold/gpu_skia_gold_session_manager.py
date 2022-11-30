# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU impl of //testing/skia_gold_common/skia_gold_session_manager.py."""

from skia_gold_common import output_managerless_skia_gold_session
from skia_gold_common import skia_gold_session_manager as sgsm


class GpuSkiaGoldSessionManager(sgsm.SkiaGoldSessionManager):
  @staticmethod
  def GetSessionClass():
    return output_managerless_skia_gold_session.OutputManagerlessSkiaGoldSession
