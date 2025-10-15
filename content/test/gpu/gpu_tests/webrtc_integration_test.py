# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import json
import itertools
from typing import Any, List
import unittest

import gpu_path_util
from gpu_tests import common_browser_args as cba
from gpu_tests import common_typing as ct
from gpu_tests import gpu_integration_test

html_path = os.path.join(gpu_path_util.CHROMIUM_SRC_DIR, 'content', 'test',
                         'data', 'gpu', 'webrtc')


class WebRTCIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  @classmethod
  def Name(cls) -> str:
    return 'webrtc'

  @classmethod
  def _SuiteSupportsParallelTests(cls) -> bool:
    return True

  @classmethod
  def GenerateGpuTests(cls, options: ct.ParsedCmdArgs) -> ct.TestGenerator:
    tests = itertools.chain(cls.GenerateWebRTCTests())
    yield from tests

  @classmethod
  def GenerateWebRTCTests(cls) -> ct.TestGenerator:
    for codec in ['H265']:
      yield (f'WebRTC_Codec_Loopback_{codec}', 'codec_loopback.html', [{
          'codec':
          f'video/{codec}',
      }])

  def RunActualGpuTest(self, test_path: str, args: ct.TestArgs) -> None:
    url = self.UrlOfStaticFilePath(os.path.join(html_path, test_path))
    tab = self.tab
    arg_obj = args[0]
    tab.Navigate(url)
    tab.action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    tab.EvaluateJavaScript('TEST.run(' + json.dumps(arg_obj) + ')')
    tab.action_runner.WaitForJavaScriptCondition('TEST.finished', timeout=60)
    if tab.EvaluateJavaScript('TEST.skipped'):
      self.skipTest('Skipping test:' + tab.EvaluateJavaScript('TEST.summary()'))
    if not tab.EvaluateJavaScript('TEST.success'):
      self.fail('Test failure:' + tab.EvaluateJavaScript('TEST.summary()'))

  @classmethod
  def SetUpProcess(cls) -> None:
    super(WebRTCIntegrationTest, cls).SetUpProcess()
    args = [
        '--use-fake-device-for-media-stream',
        '--use-fake-ui-for-media-stream',
        cba.ENABLE_EXPERIMENTAL_WEB_PLATFORM_FEATURES,
    ]

    # If we don't call CustomizeBrowserArgs cls.platform is None
    cls.CustomizeBrowserArgs(args)

    cls.StartBrowser()
    cls.SetStaticServerDirs([html_path])

  @classmethod
  def ExpectationsFiles(cls) -> List[str]:
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webrtc_expectations.txt')
    ]


def load_tests(loader: unittest.TestLoader, tests: Any,
               pattern: Any) -> unittest.TestSuite:
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
