# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import posixpath
import sys
from typing import Any, List
import unittest

from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import skia_gold_integration_test_base

import gpu_path_util


class CastStreamingIntegrationTest(
    skia_gold_integration_test_base.SkiaGoldIntegrationTestBase):
  """GPU pixel tests for Fuchsia Cast Streaming backed by Skia Gold and
  Telemetry.

  This is a separate test since this uses the cast-streaming-shell browser.
  Furthermore, this test harness actually drives test progress by configuring
  how the CastStreamingTestSender is started."""
  test_base_name = 'CastStreaming'

  @classmethod
  def Name(cls) -> str:
    """The name by which this test is invoked on the command line."""
    return 'cast_streaming'

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    namespace = pixel_test_pages.PixelTestPages
    pages = namespace.CastStreamingReceiverPages(cls.test_base_name)
    for p in pages:
      yield (p.name, posixpath.join(gpu_path_util.GPU_DATA_RELATIVE_PATH,
                                    p.url), [p])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    page = args[0]

    # This property actually comes off the class, not 'self'.
    tab = self.tab

    tab.action_runner.WaitForJavaScriptCondition(
        'document.readyState === "complete"')

    # Request a frame.
    tab.ExecuteJavaScript('domAutomationController.setFrameRequest(1)')

    # Wait until the test is done.
    tab.action_runner.WaitForJavaScriptCondition(
        'domAutomationController._done')
    has_failed = tab.EvaluateJavaScript('domAutomationController._failure')

    try:
      if has_failed:
        self.fail('page indicated test failure')
      else:
        # Actually run the test and capture the screenshot.
        screenshot = tab.Screenshot()
        self._UploadTestResultToSkiaGold(page.name, screenshot, page)
    finally:
      self._RestartBrowser('Must restart after every test')

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'cast_streaming_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
