# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import sys
import time
from typing import Any
import unittest

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test

import gpu_path_util


class NoopSleepIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls) -> str:
    return 'noop_sleep'

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    tests = (('DoNothing', 'empty.html'), )
    for t in tests:
      yield (t[0], t[1], ['_' + t[0]])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    test_name = args[0]
    tab = self.tab
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    getattr(self, test_name)(test_path)

  @classmethod
  def SetUpProcess(cls) -> None:
    super(NoopSleepIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([gpu_path_util.GPU_DATA_DIR])

  def _Navigate(self, test_path: str) -> None:
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url)

  # The browser test runner synthesizes methods with the exact name
  # given in GenerateGpuTests, so in order to hand-write our tests but
  # also go through the _RunGpuTest trampoline, the test needs to be
  # slightly differently named.
  def _DoNothing(self, test_path: str) -> None:
    self._Navigate(test_path)
    time.sleep(180)


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
