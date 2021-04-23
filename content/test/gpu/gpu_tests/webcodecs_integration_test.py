# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import time

from gpu_tests import gpu_integration_test
from gpu_tests import path_util

data_path = os.path.join(path_util.GetChromiumSrcDir(), 'content', 'test',
                         'data', 'gpu', 'webcodecs')


class WebCodecsIntegrationTest(gpu_integration_test.GpuIntegrationTest):
  @classmethod
  def Name(cls):
    return 'webcodecs'

  @classmethod
  def GenerateGpuTests(cls, options):
    yield ('WebCodecs_EncodeDecodeRender_h264_baseline',
           'encode-decode-render.html', ('{ codec : "avc1.42001E" }'))
    yield ('WebCodecs_EncodeDecodeRender_vp8', 'encode-decode-render.html',
           ('{ codec : "vp8" }'))
    yield ('WebCodecs_EncodeDecodeRender_vp9', 'encode-decode-render.html',
           ('{ codec : "vp09.00.10.08" }'))

  def RunActualGpuTest(self, test_path, *args):
    url = self.UrlOfStaticFilePath(test_path)
    tab = self.tab
    arg_obj = args[0]
    tab.Navigate(url)
    tab.action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"', timeout=5)
    tab.EvaluateJavaScript('TEST.run(' + str(arg_obj) + ')')
    tab.action_runner.WaitForJavaScriptCondition('TEST.finished', timeout=60)
    if not tab.EvaluateJavaScript('TEST.success'):
      self.fail('Test failure:' + tab.EvaluateJavaScript('TEST.summary()'))

  @classmethod
  def SetUpProcess(cls):
    super(WebCodecsIntegrationTest, cls).SetUpProcess()
    cls.CustomizeBrowserArgs(['--enable-blink-features=WebCodecs'])
    cls.StartBrowser()
    cls.SetStaticServerDirs([data_path])

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations', 'webcodecs_expectations.txt')
    ]


def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
