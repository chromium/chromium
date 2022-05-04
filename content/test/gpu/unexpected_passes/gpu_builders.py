# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' builders module."""

from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types

from chrome_telemetry_build import android_browser_types as abt


class GpuBuilders(builders.Builders):
  def __init__(self, suite, include_internal_builders):
    super().__init__(suite, include_internal_builders)
    self._isolate_names = None
    self._fake_ci_builders = None
    self._non_chromium_builders = None

  def _BuilderRunsTestOfInterest(self, test_map):
    tests = test_map.get('isolated_scripts', [])
    for t in tests:
      if t.get('isolate_name') not in self.GetIsolateNames():
        continue
      if self._suite in t.get('args', []):
        return True
    return False

  def GetIsolateNames(self):
    if self._isolate_names is None:
      self._isolate_names = {
          'telemetry_gpu_integration_test',
          'telemetry_gpu_integration_test_fuchsia',
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
          self._fake_ci_builders.setdefault(
              data_types.BuilderEntry(ci, constants.BuilderTypes.CI, False),
              set()).add(
                  data_types.BuilderEntry(try_builder,
                                          constants.BuilderTypes.TRY, False))

    return self._fake_ci_builders

  def GetNonChromiumBuilders(self):
    if self._non_chromium_builders is None:
      str_builders = {
          'Win V8 FYI Release (NVIDIA)',
          'Mac V8 FYI Release (Intel)',
          'Linux V8 FYI Release - pointer compression (NVIDIA)',
          'Linux V8 FYI Release (NVIDIA)',
          'Android V8 FYI Release (Nexus 5X)',
      }
      self._non_chromium_builders = {
          data_types.BuilderEntry(b, constants.BuilderTypes.CI, False)
          for b in str_builders
      }
    return self._non_chromium_builders
