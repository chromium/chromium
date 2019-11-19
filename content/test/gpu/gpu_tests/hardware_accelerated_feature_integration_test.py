# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

from gpu_tests import gpu_integration_test

test_harness_script = r"""
  function VerifyHardwareAccelerated(feature) {
    feature += ': '
    var list = document.querySelector('.feature-status-list');
    for (var i=0; i < list.childElementCount; i++) {
      var span_list = list.children[i].getElementsByTagName('span');
      var feature_str = span_list[0].textContent;
      var value_str = span_list[1].textContent;
      if ((feature_str == feature) &&
          (value_str == 'Hardware accelerated')) {
        return true;
      }
    }
    return false;
  };
"""

def safe_feature_name(feature):
  return feature.lower().replace(' ', '_')

class HardwareAcceleratedFeatureIntegrationTest(
    gpu_integration_test.GpuIntegrationTest):
  """Tests GPU acceleration is reported as active for various features."""

  @classmethod
  def Name(cls):
    """The name by which this test is invoked on the command line."""
    return 'hardware_accelerated_feature'

  @classmethod
  def SetUpProcess(cls):
    super(cls, HardwareAcceleratedFeatureIntegrationTest).SetUpProcess()
    cls.CustomizeBrowserArgs([])
    cls.StartBrowser()
    cls.SetStaticServerDirs([])

  def _Navigate(self, url):
    # It's crucial to use the action_runner, rather than the tab's
    # Navigate method directly. It waits for the document ready state
    # to become interactive or better, avoiding critical race
    # conditions.
    self.tab.action_runner.Navigate(
      url, script_to_evaluate_on_commit=test_harness_script)

  @classmethod
  def GenerateGpuTests(cls, options):
    tests = ('WebGL', 'Canvas')
    for feature in tests:
      yield('HardwareAcceleratedFeature_%s_accelerated' %
            safe_feature_name(feature),
            'chrome://gpu',
            (feature))

  def RunActualGpuTest(self, test_path, *args):
    feature = args[0]
    self._Navigate(test_path)
    tab = self.tab
    if not tab.EvaluateJavaScript(
        'VerifyHardwareAccelerated({{ feature }})', feature=feature):
      print 'Test failed. Printing page contents:'
      print tab.EvaluateJavaScript('document.body.innerHTML')
      self.fail('%s not hardware accelerated' % feature)

  @classmethod
  def ExpectationsFiles(cls):
    return [
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     'test_expectations',
                     'hardware_accelerated_feature_expectations.txt')]

def load_tests(loader, tests, pattern):
  del loader, tests, pattern  # Unused.
  return gpu_integration_test.LoadAllTestsInModule(sys.modules[__name__])
