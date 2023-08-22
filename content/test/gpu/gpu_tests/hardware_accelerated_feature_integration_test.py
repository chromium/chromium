# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import os
import sys
from typing import Any, List
import unittest

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test

test_harness_script = r"""
  function VerifyHardwareAccelerated(feature) {
    return getGPUInfo('feature-status-list', feature) === 'enabled';
  };
"""


def safe_feature_name(feature: str) -> str:
  return feature.lower().replace(' ', '_')


class HardwareAcceleratedFeatureIntegrationTest(
    gpu_integration_test.GpuIntegrationTest):
  """Tests GPU acceleration is reported as active for various features."""

  @classmethod
  def Name(cls) -> str:
    """The name by which this test is invoked on the command line."""
    return 'hardware_accelerated_feature'

  @classmethod
  def SetUpProcess(cls) -> None:
    super(cls, HardwareAcceleratedFeatureIntegrationTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([])

  def _Navigate(self, url: str) -> None:
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(
        url, script_to_evaluate_on_commit=test_harness_script)

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    tests = ('webgl', '2d_canvas')
    for feature in tests:
      yield ('HardwareAcceleratedFeature_%s_accelerated' %
             safe_feature_name(feature), 'chrome://gpu', [feature])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    feature = args[0]
    self._Navigate(test_path)
    tab = self.tab
    tab.WaitForJavaScriptCondition('window.gpuPagePopulated', timeout=30)
    if not tab.EvaluateJavaScript(
        'VerifyHardwareAccelerated({{ feature }})', feature=feature):
      print('Test failed. Printing page contents:')
      print(tab.EvaluateJavaScript('document.body.innerHTML'))
      self.fail('%s not hardware accelerated' % feature)

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)), 'test_expectations',
            'hardware_accelerated_feature_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
