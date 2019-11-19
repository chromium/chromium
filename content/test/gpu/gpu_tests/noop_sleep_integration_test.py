# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time

from gpu_tests import gpu_integration_test
from gpu_tests import path_util

data_path = os.path.join(
    path_util.GetChromiumSrcDir(), 'content', 'test', 'data', 'gpu')

class NoopSleepIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def Name(cls):
    return 'noop_sleep'

  @classmethod
  def GenerateGpuTests(cls, options):
    tests = (('DoNothing', 'empty.html'),)
    for t in tests:
      yield (t[0], t[1], ('_' + t[0]))

  def RunActualGpuTest(self, test_path, *args):
    test_name = args[0]
    tab = self.tab
    if not tab.browser.supports_tab_control:
      self.fail('Browser must support tab control')
    getattr(self, test_name)(test_path)

  @classmethod
  def SetUpProcess(cls):
    super(NoopSleepIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  def _Navigate(self, test_path):
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    tab.Navigate(url)

  # The browser test runner synthesizes methods with the exact name
  # given in GenerateGpuTests, so in order to hand-write our tests but
  # also go through the _RunGpuTest trampoline, the test needs to be
  # slightly differently named.
  def _DoNothing(self, test_path):
    self._Navigate(test_path)
    time.sleep(180)

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
