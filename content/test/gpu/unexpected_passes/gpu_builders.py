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
    return {}

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
