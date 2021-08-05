# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' builders module."""

from __future__ import print_function

import os
import sys

from unexpected_passes_common import builders

CHROMIUM_SRC_DIR = os.path.realpath(
    os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
TOOLS_PERF_DIR = os.path.join(CHROMIUM_SRC_DIR, 'tools', 'perf')

sys.path.append(TOOLS_PERF_DIR)
from chrome_telemetry_build import android_browser_types as abt
sys.path.remove(TOOLS_PERF_DIR)


class GpuBuilders(builders.Builders):
  def __init__(self):
    super(GpuBuilders, self).__init__()
    self._isolate_names = None
    self._fake_ci_builders = None

  def _BuilderRunsTestOfInterest(self, test_map, suite):
    tests = test_map.get('isolated_scripts', [])
    for t in tests:
      if t.get('isolate_name') not in self.GetIsolateNames():
        continue
      if suite in t.get('args', []):
        return True
    return False

  def GetIsolateNames(self):
    if self._isolate_names is None:
      self._isolate_names = {
          'fuchsia_telemetry_gpu_integration_test',
          'telemetry_gpu_integration_test',
      }
      # Android targets are split based on binary type, so add those using the
      # maintained list of suffixes.
      for suffix in abt.TELEMETRY_ANDROID_BROWSER_TARGET_SUFFIXES:
        self._isolate_names.add('telemetry_gpu_integration_test' + suffix)
    return self._isolate_names

  def GetFakeCiBuilders(self):
    if self._fake_ci_builders is None:
      # Go from try -> CI then reverse the mapping so that there's less of a
      # chance of typos being introduced in the repeated trybot names.
      fake_try_builders = {
          # chromium.gpu.fyi
          'android_angle_rel_ng': [
              'ANGLE GPU Android Release (Nexus 5X)',
          ],
          'android_optional_gpu_tests_rel': [
              'Optional Android Release (Nexus 5X)',
              'Optional Android Release (Pixel 4)',
          ],
          'linux-angle-rel': [
              'ANGLE GPU Linux Release (Intel HD 630)',
              'ANGLE GPU Linux Release (NVIDIA)',
          ],
          'linux_optional_gpu_tests_rel': [
              'Optional Linux Release (Intel HD 630)',
              'Optional Linux Release (NVIDIA)',
          ],
          'mac_optional_gpu_tests_rel': [
              'Optional Mac Release (Intel)',
              'Optional Mac Retina Release (AMD)',
              'Optional Mac Retina Release (NVIDIA)',
          ],
          'win_optional_gpu_tests_rel': [
              'Optional Win10 x64 Release (Intel HD 630)',
              'Optional Win10 x64 Release (NVIDIA)',
          ],
      }

      self._fake_ci_builders = {}
      for try_builder, ci_builder_list in fake_try_builders.items():
        for ci in ci_builder_list:
          self._fake_ci_builders.setdefault(ci, set()).add(try_builder)

    return self._fake_ci_builders

  def GetNonChromiumBuilders(self):
    return {
        'Win V8 FYI Release (NVIDIA)',
        'Mac V8 FYI Release (Intel)',
        'Linux V8 FYI Release - pointer compression (NVIDIA)',
        'Linux V8 FYI Release (NVIDIA)',
        'Android V8 FYI Release (Nexus 5X)',
    }
