# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GPU-specific implementation of the unexpected passes' builders module."""

from typing import Any, Dict, Optional, Set

from unexpected_passes_common import builders
from unexpected_passes_common import constants
from unexpected_passes_common import data_types

from chrome_telemetry_build import android_browser_types as abt


class GpuBuilders(builders.Builders):
  def __init__(self, suite: str, include_internal_builders: bool):
    super().__init__(suite, include_internal_builders)
    self._isolate_names: Optional[Set[str]] = None
    self._non_chromium_builders: Optional[Set[data_types.BuilderEntry]] = None

  def _BuilderRunsTestOfInterest(self, test_map: Dict[str, Any]) -> bool:
    # Builders running tests in Chrome Labs.
    tests = test_map.get('isolated_scripts', [])
    for t in tests:
      if t.get('test') not in self.GetIsolateNames():
        continue
      if self._suite in t.get('args', []):
        return True

    # Builders running tests in Skylab.
    tests = test_map.get('skylab_tests', [])
    for t in tests:
      if t.get('test') not in self.GetIsolateNames():
        continue
      if self._suite in t.get('args', []):
        return True

    return False

  def GetIsolateNames(self) -> Set[str]:
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

  def GetFakeCiBuilders(self) -> builders.FakeBuildersDict:
    return {}

  def GetNonChromiumBuilders(self) -> Set[data_types.BuilderEntry]:
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
