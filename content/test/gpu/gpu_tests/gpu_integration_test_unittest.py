# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# It's reasonable for unittests to be messing with protected members.
# Additionally, there is a lot to test, so line count is expected to be large.
# pylint: disable=protected-access,too-many-lines


import copy
import dataclasses
import json
import os
import tempfile
import typing
from typing import Any, Type
import unittest
from unittest import mock

# Needed in order for the tools/perf and Telemetry imports to work.
# pylint: disable=unused-import,wrong-import-order
from gpu_path_util import setup_telemetry_paths
from gpu_path_util import setup_tools_perf_paths
# pylint: enable=unused-import,wrong-import-order
from chrome_telemetry_build import chromium_config
from py_utils import tempfile_ext
from telemetry.internal.browser import browser_options as bo
from telemetry.internal.util import binary_manager
from telemetry.internal.platform import system_info
from telemetry.testing import browser_test_runner
from telemetry.testing import fakes
from telemetry.testing import run_browser_tests

import gpu_path_util
import gpu_project_config
from gpu_tests import common_typing as ct
from gpu_tests import context_lost_integration_test
from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import trace_integration_test as trace_it
from gpu_tests import webgl1_conformance_integration_test as webgl1_cit
from gpu_tests import webgl2_conformance_integration_test as webgl2_cit
from gpu_tests import webgpu_compat_cts_integration_test as webgpu_compat_cit
import run_gpu_integration_test

# Unittest test cases are defined as public methods, so ignore complaints about
# having too many.
# pylint: disable=too-many-public-methods

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

VENDOR_STRING_IMAGINATION = 'Imagination Technologies'
DEVICE_STRING_SGX = 'PowerVR SGX 554'

GpuTestClassType = Type[gpu_integration_test.GpuIntegrationTest]


def _GetSystemInfo(  # pylint: disable=too-many-arguments
    gpu: int = 0,
    device: int = 0,
    vendor_string: str = '',
    device_string: str = '',
    passthrough: bool = False,
    gl_renderer: str = '',
    is_asan: bool = False,
    is_clang_coverage: bool = False,
    target_cpu_bits: int = 64) -> system_info.SystemInfo:
  sys_info = {
      'model_name': '',
      'gpu': {
          'devices': [
              {
                  'vendor_id': gpu,
                  'device_id': device,
                  'vendor_string': vendor_string,
                  'device_string': device_string
              },
          ],
          'aux_attributes': {
              'passthrough_cmd_decoder': passthrough,
              'is_asan': is_asan,
              'is_clang_coverage': is_clang_coverage,
              'target_cpu_bits': target_cpu_bits
          },
          'feature_status': {
              'gpu_compositing': 'enabled',
              'opengl': 'enabled_on'
          }
      }
  }
  if gl_renderer:
    sys_info['gpu']['aux_attributes']['gl_renderer'] = gl_renderer
  return system_info.SystemInfo.FromDict(sys_info)


def _GetTagsToTest(browser: fakes.FakeBrowser,
                   test_class: GpuTestClassType | None = None) -> set[str]:
  browser = typing.cast(ct.Browser, browser)
  test_class = test_class or gpu_integration_test.GpuIntegrationTest
  tags = None
  with mock.patch.object(
      test_class, 'ExpectationsFiles', return_value=['exp.txt']):
    tags = set(test_class.GetPlatformTags(browser))
  return tags


def _GenerateNvidiaExampleTagsForTestClassAndArgs(
    test_class: GpuTestClassType,
    args: mock.MagicMock,
    is_asan: bool = False,
    is_clang_coverage: bool = False,
    target_cpu_bits: int = 64,
) -> set[str]:
  tags = None
  with mock.patch.object(
      test_class, 'ExpectationsFiles', return_value=['exp.txt']):
    _ = list(test_class.GenerateTestCases__RunGpuTest(args))
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA,
        device=0x1cb3,
        gl_renderer='ANGLE Direct3D9',
        is_asan=is_asan,
        is_clang_coverage=is_clang_coverage,
        target_cpu_bits=target_cpu_bits)
    tags = _GetTagsToTest(browser, test_class)
  return tags


@dataclasses.dataclass
class _IntegrationTestArgs():
  """Struct-like object for defining an integration test."""
  test_name: str
  failures: list[str] = ct.EmptyList()
  successes: list[str] = ct.EmptyList()
  skips: list[str] = ct.EmptyList()
  additional_args: list[str] = ct.EmptyList()


class GpuIntegrationTestUnittest(unittest.TestCase):
  def setUp(self) -> None:
    self._test_state = {}
    self._test_result = {}

  def _RunGpuIntegrationTests(self,
                              test_name: str,
                              extra_args: list[str] | None = None) -> None:
    extra_args = extra_args or []
    unittest_config = chromium_config.ChromiumConfig(
        top_level_dir=gpu_path_util.GPU_DIR,
        benchmark_dirs=[os.path.join(gpu_path_util.GPU_DIR, 'unittest_data')])
    with binary_manager.TemporarilyReplaceBinaryManager(None), \
         mock.patch.object(gpu_project_config, 'CONFIG', unittest_config):
      # TODO(crbug.com/40139419): Using NamedTemporaryFile() as a generator is
      # causing windows bots to fail. When the issue is fixed with
      # tempfile_ext.NamedTemporaryFile(), put it in the list of generators
      # starting this with block. Also remove the try finally statement
      # below.
      with tempfile.NamedTemporaryFile(delete=False) as temp_file:
        temp_file_name = temp_file.name
      try:
        test_argv = [
            test_name,
            f'--write-full-results-to={temp_file_name}',
            # We don't want the underlying typ-based tests to report their
            # results to ResultDB.
            '--disable-resultsink',
            # These tests currently rely on some information sticking around
            # between tests, so we need to use the older global process pool
            # approach instead of having different pools scoped for
            # parallel/serial execution.
            '--use-global-pool',
        ] + extra_args
        processed_args = run_gpu_integration_test.ProcessArgs(test_argv)
        telemetry_args = browser_test_runner.ProcessConfig(
            unittest_config, processed_args)
        run_browser_tests.RunTests(telemetry_args)
        with open(temp_file_name, encoding='utf-8') as f:
          self._test_result = json.load(f)
      finally:
        os.remove(temp_file_name)

  def testOverrideDefaultRetryArgumentsinRunGpuIntegrationTests(self) -> None:
    self._RunGpuIntegrationTests('run_tests_with_expectations_files',
                                 ['--retry-limit=1'])
    self.assertEqual(
        self._test_result['tests']['a']['b']['unexpected-fail.html']['actual'],
        'FAIL FAIL')

  def testDefaultRetryArgumentsinRunGpuIntegrationTests(self) -> None:
    self._RunGpuIntegrationTests('run_tests_with_expectations_files')
    self.assertEqual(
        self._test_result['tests']['a']['b']['expected-flaky.html']['actual'],
        'FAIL FAIL FAIL')

  def testTestNamePrefixGenerationInRunGpuIntegrationTests(self) -> None:
    self._RunGpuIntegrationTests('simple_integration_unittest')
    self.assertIn('expected_failure', self._test_result['tests'])

  # pylint: disable=too-many-arguments
  def _TestTagGenerationForMockPlatform(self,
                                        test_class: GpuTestClassType,
                                        args: mock.MagicMock,
                                        is_asan: bool = False,
                                        is_clang_coverage: bool = False,
                                        target_cpu_bits: int = 64) -> set[str]:
    tag_set = _GenerateNvidiaExampleTagsForTestClassAndArgs(
        test_class, args, is_asan, is_clang_coverage, target_cpu_bits)
    self.assertTrue(
        set([
            'win', 'win10', 'angle-d3d9', 'release', 'nvidia', 'nvidia-0x1cb3',
            'no-passthrough'
        ]).issubset(tag_set))
    return tag_set
  # pylint: enable=too-many-arguments

  def testGenerateContextLostExampleTagsForAsan(self) -> None:
    args = gpu_helper.GetMockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args,
        is_asan=True)
    self.assertIn('asan', tag_set)
    self.assertNotIn('no-asan', tag_set)

  def testGenerateContextLostExampleTagsForNoAsan(self) -> None:
    args = gpu_helper.GetMockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args,
        is_asan=False)
    self.assertIn('no-asan', tag_set)
    self.assertNotIn('asan', tag_set)

  def testGenerateContextLostExampleTagsForClangCoverage(self) -> None:
    args = gpu_helper.GetMockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args,
        is_clang_coverage=True)
    self.assertIn('clang-coverage', tag_set)
    self.assertNotIn('no-clang-coverage', tag_set)

  def testGenerateContextLostExampleTagsForNoClangCoverage(self) -> None:
    args = gpu_helper.GetMockArgs()
    tag_set = self._TestTagGenerationForMockPlatform(
        context_lost_integration_test.ContextLostIntegrationTest,
        args,
        is_clang_coverage=False)
    self.assertIn('no-clang-coverage', tag_set)
    self.assertNotIn('clang-coverage', tag_set)

  def testGenerateContextLostExampleTagsForTargetCpu(self) -> None:
    args = gpu_helper.GetMockArgs()
    self.assertIn(
        'target-cpu-64',
        self._TestTagGenerationForMockPlatform(
            context_lost_integration_test.ContextLostIntegrationTest,
            args,
            target_cpu_bits=64))
    self.assertIn(
        'target-cpu-32',
        self._TestTagGenerationForMockPlatform(
            context_lost_integration_test.ContextLostIntegrationTest,
            args,
            target_cpu_bits=32))
    self.assertIn(
        'target-cpu-31',
        self._TestTagGenerationForMockPlatform(
            context_lost_integration_test.ContextLostIntegrationTest,
            args,
            target_cpu_bits=31))

  def testGenerateWebglConformanceExampleTagsForAsan(self) -> None:
    args = gpu_helper.GetMockArgs(webgl_version='1.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl1_cit.WebGL1ConformanceIntegrationTest, args, is_asan=True)
    self.assertTrue(set(['asan']).issubset(tag_set))
    self.assertFalse(set(['no-asan']) & tag_set)

  def testGenerateWebglConformanceExampleTagsForNoAsan(self) -> None:
    args = gpu_helper.GetMockArgs(webgl_version='2.0.0')
    tag_set = self._TestTagGenerationForMockPlatform(
        webgl2_cit.WebGL2ConformanceIntegrationTest, args)
    self.assertTrue(set(['no-asan']).issubset(tag_set))
    self.assertFalse(set(['asan']) & tag_set)

  def testWebGlConformanceTimeoutNoAsan(self) -> None:
    instance = webgl1_cit.WebGL1ConformanceIntegrationTest(
        '_RunConformanceTest')
    instance._is_asan = False
    self.assertEqual(instance._GetTestTimeout(), 300)

  def testWebGlConformanceTimeoutAsan(self) -> None:
    instance = webgl1_cit.WebGL1ConformanceIntegrationTest(
        '_RunConformanceTest')
    instance._is_asan = True
    self.assertEqual(instance._GetTestTimeout(), 600)

  def testAsanClassMemberSetCorrectly(self):
    test_class = gpu_integration_test.GpuIntegrationTest
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser = typing.cast(ct.Browser, browser)

    browser._returned_system_info = _GetSystemInfo(is_asan=True)
    with mock.patch.object(test_class,
                           'ExpectationsFiles',
                           return_value=['exp.txt']):
      test_class.GetPlatformTags(browser)
    self.assertTrue(test_class._is_asan)

    browser._returned_system_info = _GetSystemInfo(is_asan=False)
    with mock.patch.object(test_class,
                           'ExpectationsFiles',
                           return_value=['exp.txt']):
      test_class.GetPlatformTags(browser)
    self.assertFalse(test_class._is_asan)

  @mock.patch('gpu_tests.util.host_information.IsLinux', return_value=False)
  @mock.patch('gpu_tests.util.host_information.GetSystemMemoryBytes',
              return_value=32_000_000_000)
  @mock.patch('gpu_tests.util.host_information.IsArmCpu',
              return_value=False)
  def testGenerateNvidiaExampleTags(self, _, __, ___) -> None:
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x1cb3, gl_renderer='ANGLE Direct3D9')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'win',
            'win10',
            'release',
            'nvidia',
            'nvidia-0x1cb3',
            'angle-d3d9',
            'no-passthrough',
            'renderer-skia-gl',
            'no-asan',
            'arch-x86_64',
            'target-cpu-64',
            'no-clang-coverage',
            'graphite-disabled',
            'memory_ge_16gb',
        ]))

  @mock.patch('gpu_tests.util.host_information.IsLinux', return_value=False)
  @mock.patch('gpu_tests.util.host_information.GetSystemMemoryBytes',
              return_value=32_000_000_000)
  @mock.patch('gpu_tests.util.host_information.IsArmCpu',
              return_value=False)
  def testGenerateVendorTagUsingVendorString(self, _, __, ___) -> None:
    platform = fakes.FakePlatform('mac', 'mojave')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        vendor_string=VENDOR_STRING_IMAGINATION,
        device_string=DEVICE_STRING_SGX,
        passthrough=True,
        gl_renderer='ANGLE OpenGL ES')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'mac',
            'mojave',
            'release',
            'imagination',
            'no-asan',
            'arch-x86_64',
            'target-cpu-64',
            'imagination-PowerVR-SGX-554',
            'angle-opengles',
            'passthrough',
            'renderer-skia-gl',
            'no-clang-coverage',
            'graphite-disabled',
            'memory_ge_16gb',
        ]))

  @mock.patch('gpu_tests.util.host_information.IsLinux', return_value=False)
  @mock.patch('gpu_tests.util.host_information.GetSystemMemoryBytes',
              return_value=32_000_000_000)
  @mock.patch('gpu_tests.util.host_information.IsArmCpu',
              return_value=False)
  def testGenerateVendorTagUsingDeviceString(self, _, __, ___) -> None:
    platform = fakes.FakePlatform('mac', 'mojave')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        vendor_string='illegal vendor string',
        device_string='ANGLE (Imagination, Triangle Monster 3000, 1.0)')
    self.assertEqual(
        _GetTagsToTest(browser),
        set([
            'mac',
            'mojave',
            'release',
            'imagination',
            'no-asan',
            'arch-x86_64',
            'target-cpu-64',
            'imagination-Triangle-Monster-3000',
            'angle-disabled',
            'no-passthrough',
            'renderer-skia-gl',
            'no-clang-coverage',
            'graphite-disabled',
            'memory_ge_16gb',
        ]))

  @mock.patch('gpu_tests.util.host_information.IsLinux', return_value=False)
  def testGenerateMemoryTags(self, _):
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x1cb3, gl_renderer='ANGLE Direct3D9')

    with mock.patch('gpu_tests.util.host_information.GetSystemMemoryBytes',
                    return_value=8_000_000_000):
      tags = _GetTagsToTest(browser)
      self.assertIn('memory_lt_16gb', tags)
      self.assertNotIn('memory_ge_16gb', tags)

    with mock.patch('gpu_tests.util.host_information.GetSystemMemoryBytes',
                    return_value=16_000_000_000):
      tags = _GetTagsToTest(browser)
      self.assertNotIn('memory_lt_16gb', tags)
      self.assertIn('memory_ge_16gb', tags)

    # Ensure that tags aren't generated on remote platforms.
    platform = fakes.FakePlatform('android', 'android-15')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x0000, gl_renderer='ANGLE OpenGL ES')
    tags = _GetTagsToTest(browser)
    for t in tags:
      self.assertFalse(t.startswith('memory_'))

  @mock.patch('gpu_tests.util.host_information.IsLinux', return_value=False)
  def testGenerateArchitectureTags(self, _):
    platform = fakes.FakePlatform('win', 'win10')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x1cb3, gl_renderer='ANGLE Direct3D9')

    with mock.patch('gpu_tests.util.host_information.IsArmCpu',
                    return_value=True):
      tags = _GetTagsToTest(browser)
      self.assertIn('arch-arm64', tags)
      self.assertNotIn('arch-x86_64', tags)

    with mock.patch('gpu_tests.util.host_information.IsArmCpu',
                    return_value=False):
      tags = _GetTagsToTest(browser)
      self.assertNotIn('arch-arm64', tags)
      self.assertIn('arch-x86_64', tags)

    # Ensure that tags aren't generated on remote platforms.
    platform = fakes.FakePlatform('android', 'android-15')
    browser = fakes.FakeBrowser(platform, 'release')
    browser._returned_system_info = _GetSystemInfo(
        gpu=VENDOR_NVIDIA, device=0x0000, gl_renderer='ANGLE OpenGL ES')
    tags = _GetTagsToTest(browser)
    for t in tags:
      self.assertFalse(t.startswith('arch-'))

  @mock.patch.dict(os.environ, clear=True)
  def testGenerateDisplayServer(self) -> None:
    platform = fakes.FakePlatform('mac', 'mojave')
    browser = fakes.FakeBrowser(platform, 'release')
    browser = typing.cast(ct.Browser, browser)

    with mock.patch('gpu_tests.util.host_information.IsLinux',
                    return_value=False):
      tags = gpu_integration_test.GpuIntegrationTest.GetPlatformTags(browser)
      for t in tags:
        self.assertFalse(t.startswith('display-server'))

    with mock.patch('gpu_tests.util.host_information.IsLinux',
                    return_value=True):
      tags = gpu_integration_test.GpuIntegrationTest.GetPlatformTags(browser)
      self.assertIn('display-server-x', tags)

      os.environ['WAYLAND_DISPLAY'] = 'wayland-0'
      tags = gpu_integration_test.GpuIntegrationTest.GetPlatformTags(browser)
      self.assertIn('display-server-wayland', tags)

  def testTraceTestPrefixesInSync(self):
    """Verifies that the trace test known prefix list is in sync."""
    test_cases = list(
        trace_it.TraceIntegrationTest.GenerateTestCases__RunGpuTest(
            mock.MagicMock()))
    valid_prefixes = tuple(trace_it.TraceIntegrationTest.known_test_prefixes)
    for test_name, _ in test_cases:
      self.assertTrue(test_name.startswith(valid_prefixes))

  def testSimpleIntegrationTest(self) -> None:
    test_args = _IntegrationTestArgs('simple_integration_unittest')
    test_args.failures = [
        'unexpected_error',
        'unexpected_failure',
    ]
    test_args.successes = [
        'expected_flaky',
        'expected_failure',
    ]
    test_args.skips = ['expected_skip']
    test_args.additional_args = [
        '--retry-only-retry-on-failure',
        '--retry-limit=3',
        '--test-name-prefix=unittest_data.integration_tests.SimpleTest.',
    ]

    self._RunIntegrationTest(test_args)
    # The number of browser starts include the one call to StartBrowser at the
    # beginning of the run of the test suite and for each RestartBrowser call
    # which happens after every failure
    self.assertEqual(self._test_state['num_browser_starts'], 6)

  def testIntegrationTesttWithBrowserFailure(self) -> None:
    test_args = _IntegrationTestArgs(
        'browser_start_failure_integration_unittest')
    test_args.successes = [
        'unittest_data.integration_tests.BrowserStartFailureTest.restart'
    ]

    self._RunIntegrationTest(test_args)
    self.assertEqual(self._test_state['num_browser_crashes'], 2)
    self.assertEqual(self._test_state['num_browser_starts'], 3)

  def testIntegrationTestWithBrowserCrashUponStart(self) -> None:
    test_args = _IntegrationTestArgs(
        'browser_crash_after_start_integration_unittest')
    test_args.successes = [
        'unittest_data.integration_tests.BrowserCrashAfterStartTest.restart'
    ]

    self._RunIntegrationTest(test_args)
    self.assertEqual(self._test_state['num_browser_crashes'], 2)
    self.assertEqual(self._test_state['num_browser_starts'], 3)

  def testRetryLimit(self) -> None:
    test_args = _IntegrationTestArgs('test_retry_limit')
    test_args.failures = [
        'unittest_data.integration_tests.TestRetryLimit.unexpected_failure'
    ]
    test_args.additional_args = ['--retry-limit=2']

    self._RunIntegrationTest(test_args)
    # The number of attempted runs is 1 + the retry limit.
    self.assertEqual(self._test_state['num_test_runs'], 3)

  def _RunTestsWithExpectationsFiles(self) -> None:
    test_args = _IntegrationTestArgs('run_tests_with_expectations_files')
    test_args.failures = ['a/b/unexpected-fail.html']
    test_args.successes = [
        'a/b/expected-fail.html',
        'a/b/expected-flaky.html',
    ]
    test_args.skips = ['should_skip']
    test_args.additional_args = [
        '--retry-limit=3',
        '--retry-only-retry-on-failure-tests',
        ('--test-name-prefix=unittest_data.integration_tests.'
         'RunTestsWithExpectationsFiles.'),
    ]

    self._RunIntegrationTest(test_args)

  def testTestFilterCommandLineArg(self) -> None:
    test_args = _IntegrationTestArgs('run_tests_with_expectations_files')
    test_args.failures = ['a/b/unexpected-fail.html']
    test_args.successes = ['a/b/expected-fail.html']
    test_args.skips = ['should_skip']
    test_args.additional_args = [
        '--retry-limit=3',
        '--retry-only-retry-on-failure-tests',
        ('--test-filter=a/b/unexpected-fail.html::a/b/expected-fail.html::'
         'should_skip'),
        ('--test-name-prefix=unittest_data.integration_tests.'
         'RunTestsWithExpectationsFiles.'),
    ]

    self._RunIntegrationTest(test_args)

  def testUseTestExpectationsFileToHandleExpectedSkip(self) -> None:
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['should_skip']
    self.assertEqual(results['expected'], 'SKIP')
    self.assertEqual(results['actual'], 'SKIP')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleUnexpectedTestFailure(self) -> None:
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['unexpected-fail.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFailure(self) -> None:
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-fail.html']
    self.assertEqual(results['expected'], 'FAIL')
    self.assertEqual(results['actual'], 'FAIL')
    self.assertNotIn('is_regression', results)

  def testUseTestExpectationsFileToHandleExpectedFlakyTest(self) -> None:
    self._RunTestsWithExpectationsFiles()
    results = self._test_result['tests']['a']['b']['expected-flaky.html']
    self.assertEqual(results['expected'], 'PASS')
    self.assertEqual(results['actual'], 'FAIL FAIL FAIL PASS')
    self.assertNotIn('is_regression', results)

  def testRepeat(self) -> None:
    test_args = _IntegrationTestArgs('test_repeat')
    test_args.successes = ['unittest_data.integration_tests.TestRepeat.success']
    test_args.additional_args = ['--repeat=3']

    self._RunIntegrationTest(test_args)
    self.assertEqual(self._test_state['num_test_runs'], 3)

  def testAlsoRunDisabledTests(self) -> None:
    test_args = _IntegrationTestArgs('test_also_run_disabled_tests')
    test_args.failures = [
        'skip',
        'flaky',
    ]
    # Tests that are expected to fail and do fail are treated as test passes
    test_args.successes = ['expected_failure']
    test_args.additional_args = [
        '--all',
        '--test-name-prefix',
        'unittest_data.integration_tests.TestAlsoRunDisabledTests.',
        '--retry-limit=3',
        '--retry-only-retry-on-failure',
    ]

    self._RunIntegrationTest(test_args)
    self.assertEqual(self._test_state['num_flaky_test_runs'], 4)
    self.assertEqual(self._test_state['num_test_runs'], 6)

  def testStartBrowser_Retries(self) -> None:
    class TestException(Exception):
      pass

    def SetBrowserAndRaiseTestException() -> None:
      gpu_integration_test.GpuIntegrationTest.browser = mock.MagicMock()
      raise TestException

    gpu_integration_test.GpuIntegrationTest.browser = None
    gpu_integration_test.GpuIntegrationTest.platform = None
    gpu_integration_test.GpuIntegrationTest._finder_options = mock.MagicMock()
    with mock.patch.object(
        gpu_integration_test.serially_executed_browser_test_case.\
            SeriallyExecutedBrowserTestCase,
        'StartBrowser',
        side_effect=SetBrowserAndRaiseTestException) as mock_start_browser:
      with mock.patch.object(gpu_integration_test.GpuIntegrationTest,
                             'StopBrowser') as mock_stop_browser:
        with self.assertRaises(TestException):
          gpu_integration_test.GpuIntegrationTest.StartBrowser()
        self.assertEqual(mock_start_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)
        self.assertEqual(mock_stop_browser.call_count,
                         gpu_integration_test._START_BROWSER_RETRIES)

  def _RunIntegrationTest(self, test_args: _IntegrationTestArgs) -> None:
    """Runs an integration and asserts fail/success/skip expectations.

    Args:
      test_args: A _IntegrationTestArgs instance to use.
    """
    config = chromium_config.ChromiumConfig(
        top_level_dir=gpu_path_util.GPU_DIR,
        benchmark_dirs=[os.path.join(gpu_path_util.GPU_DIR, 'unittest_data')])

    with binary_manager.TemporarilyReplaceBinaryManager(None), \
         tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      test_results_path = os.path.join(temp_dir, 'test_results.json')
      test_state_path = os.path.join(temp_dir, 'test_state.json')
      # We are processing ChromiumConfig instance and getting the argument
      # list. Then we pass it directly to run_browser_tests.RunTests. If
      # we called browser_test_runner.Run, then it would spawn another
      # subprocess which is less efficient.
      args = browser_test_runner.ProcessConfig(
          config,
          [
              test_args.test_name,
              f'--write-full-results-to={test_results_path}',
              f'--test-state-json-path={test_state_path}',
              # We don't want the underlying typ-based tests to report their
              # results to ResultDB.
              '--disable-resultsink',
              # These tests currently rely on some information sticking around
              # between tests, so we need to use the older global process pool
              # approach instead of having different pools scoped for
              # parallel/serial execution.
              '--use-global-pool',
          ] + test_args.additional_args)
      run_browser_tests.RunTests(args)
      with open(test_results_path, encoding='utf-8') as f:
        self._test_result = json.load(f)
      with open(test_state_path, encoding='utf-8') as f:
        self._test_state = json.load(f)
      actual_successes, actual_failures, actual_skips = (_ExtractTestResults(
          self._test_result))
      self.assertEqual(set(actual_failures), set(test_args.failures))
      self.assertEqual(set(actual_successes), set(test_args.successes))
      self.assertEqual(set(actual_skips), set(test_args.skips))


def RunFakeBrowserStartWithArgsAndGpuInfo(
    additional_args: list[str],
    gpu_info: Any,
    gpu_test_class: GpuTestClassType = gpu_integration_test.GpuIntegrationTest
) -> None:
  cls = gpu_test_class

  def FakeStartBrowser():
    cls.browser = mock.Mock()
    cls.browser.tabs = [mock.Mock()]
    mock_system_info = mock.Mock()
    mock_system_info.gpu = gpu_info
    cls.browser.GetSystemInfo = mock.Mock(return_value=mock_system_info)

  with mock.patch(
      'telemetry.testing.serially_executed_browser_test_case.'
      'SeriallyExecutedBrowserTestCase.StartBrowser',
      side_effect=FakeStartBrowser):
    options = fakes.CreateBrowserFinderOptions()
    cls._finder_options = options
    cls._original_finder_options = options
    cls.platform = None
    cls.CustomizeBrowserArgs(additional_args)
    cls.StartBrowser()


def CreateGpuInfo(aux_attributes: dict | None = None,
                  feature_statuses: dict | None = None) -> mock.Mock:
  aux_attributes = aux_attributes or {}
  feature_statuses = feature_statuses or {}

  gpu_info = mock.Mock()
  gpu_info.aux_attributes = aux_attributes
  gpu_info.feature_status = feature_statuses
  device = mock.Mock()
  device.device_string = 'device_string'
  gpu_info.devices = [device]
  return gpu_info


# TODO(crbug.com/372740546): Find a way to properly unittest the cases
# where --gpu-disabled is passed in as well. Currently, we run into
# problems due to cls.platform being None, which causes problems with
# GPU code in _GenerateAndSanitizeBrowserArgs. Setting cls.platform to
# non-None values causes Telemetry code to fail due to the platform
# changing when it expects it to stay constant throughout the entire
# suite.
class FeatureVerificationUnittest(unittest.TestCase):

  # pylint: disable=no-self-use
  def testVerifyGLBackendSuccessUnspecified(self):
    """Tests GL backend verification that passes w/o a backend specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo([], gpu_info)

  def testVerifyGLBackendSuccessSpecified(self):
    """Tests GL backend verification that passes w/ a backend specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(['--use-gl=angle'], gpu_info)

  def testVerifyGLBackendFailure(self):
    """Tests GL backend verification that fails."""
    gpu_info = CreateGpuInfo(aux_attributes={})
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested GL backend \\(angle\\) had no effect on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--use-gl=angle'], gpu_info)

  def testVerifyANGLEBackendSuccessUnspecified(self):
    """Tests ANGLE backend verification that passes w/o a backend specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo([], gpu_info)

  def testVerifyANGLEBackendSuccessSpecified(self):
    """Tests ANGLE backend verification that passes w/ a backend specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(['--use-angle=gl'], gpu_info)

  def testVerifyANGLEBackendFailureUnknownBackend(self):
    """Tests ANGLE backend verification failure due to an unknown backend."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE foo',
    })
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested ANGLE backend \\(foo\\) had no effect on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--use-angle=foo'], gpu_info)

  def testVerifyANGLEBackendFailureMismatchedBackend(self):
    """Tests ANGLE backend verification failure due to mismatched backends."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE Vulkan',
    })
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested ANGLE backend \\(gl\\) had no effect on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--use-angle=gl'], gpu_info)

  def testVerifyCommandDecoderSuccessUnspecified(self):
    """Tests cmd decoder verification that passes w/o a decoder specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'passthrough_cmd_decoder': True,
    })
    RunFakeBrowserStartWithArgsAndGpuInfo([], gpu_info)

  def testVerifyCommandDecoderSuccessSpecified(self):
    """Tests cmd decoder verification that passes w/ a decoder specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'passthrough_cmd_decoder': True,
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(['--use-cmd-decoder=passthrough'],
                                          gpu_info)

  def testVerifyCommandDecoderFailureUnknownDecoder(self):
    """Tests cmd decoder verification that fails due to an unknown decoder."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'passthrough_cmd_decoder': True,
    })
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested command decoder \\(foo\\) had no effect on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--use-cmd-decoder=foo'], gpu_info)

  def testVerifyCommandDecoderFailureMismatchedDecoder(self):
    """Tests cmd decoder verification that fails due to a mismatched decoder."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'passthrough_cmd_decoder': False,
    })
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested command decoder \\(passthrough\\) had no effect on the '
        'browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--use-cmd-decoder=passthrough'],
                                            gpu_info)

  def testVerifySkiaGraphiteSuccessUnspecified(self):
    """Tests Skia Graphite verification that passes w/o specification."""
    gpu_info = CreateGpuInfo(feature_statuses={
        'skia_graphite': 'enabled_on',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo([], gpu_info)

  def testVerifySkiaGraphiteSuccessSpecified(self):
    """Tests Skia Graphite verification that passes w/ specification."""
    gpu_info = CreateGpuInfo(feature_statuses={
        'skia_graphite': 'enabled_on',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(['--enable-features=SkiaGraphite'],
                                          gpu_info)

    gpu_info = CreateGpuInfo(feature_statuses={})
    RunFakeBrowserStartWithArgsAndGpuInfo(['--disable-features=SkiaGraphite'],
                                          gpu_info)

  def testVerifySkiaGraphiteFailureMismatchedStatus(self):
    """Tests Skia Graphite verification that fails due to mismatched status."""
    gpu_info = CreateGpuInfo(feature_statuses={})
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested Skia Graphite status \\(graphite-enabled\\) had no effect '
        'on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--enable-features=SkiaGraphite'],
                                            gpu_info)

    gpu_info = CreateGpuInfo(feature_statuses={
        'skia_graphite': 'enabled_on',
    })
    with self.assertRaisesRegex(
        RuntimeError,
        'Requested Skia Graphite status \\(graphite-disabled\\) had no effect '
        'on the browser:.*'):
      RunFakeBrowserStartWithArgsAndGpuInfo(['--disable-features=SkiaGraphite'],
                                            gpu_info)

  # pylint: enable=no-self-use


class FeatureVerificationWebGPUCompatUnittest(unittest.TestCase):

  # pylint: disable=no-self-use
  def testVerifyCompatContextSuccessUnspecified(self):
    """Tests WebGPU compat verification that passes w/o the es31 flag."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL ES 3.1',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(
        [], gpu_info, webgpu_compat_cit.WebGpuCompatCtsIntegrationTest)

  def testVerifyCompatContextSuccessSpecified(self):
    """Tests WebGPU compat verification that passes w/ the es31 flag."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'ANGLE OpenGL ES 3.1',
    })
    RunFakeBrowserStartWithArgsAndGpuInfo(
        ['--use-dawn-features=gl_force_es_31_and_no_extensions'], gpu_info,
        webgpu_compat_cit.WebGpuCompatCtsIntegrationTest)

  # TODO(crbug.com/388318201): Uncomment the following test
  # def testVerifyCompatContextFailure(self):
  #   """Tests WebGPU compat verification that fails."""
  #   gpu_info = CreateGpuInfo(aux_attributes={
  #       'gl_renderer': 'ANGLE OpenGL ES 3.2',
  #   })
  #   with self.assertRaisesRegex(RuntimeError,
  #                             'Requested WebGPU compat context min ES31 .*'):
  #     RunFakeBrowserStartWithArgsAndGpuInfo(
  #         ['--use-dawn-features=gl_force_es_31_and_no_extensions'], gpu_info,
  #         webgpu_compat_cit.WebGpuCompatCtsIntegrationTest)

class PreemptArgsUnittest(unittest.TestCase):

  def testNoConflictIsNoOp(self):
    """Tests that no conflict arguments results in a no-op."""
    options = bo.BrowserOptions()
    options.AppendExtraBrowserArgs(['--use-angle=gl', '--another-arg'])
    expected_browser_args = copy.deepcopy(options.extra_browser_args)
    gpu_integration_test._PreemptArguments(options,
                                           ['--use-webgpu-adapter=swiftshader'])
    self.assertEqual(options.extra_browser_args, expected_browser_args)

  def testConflictingArgsRemoved(self):
    for arg in gpu_integration_test._ARGS_TO_PREEMPT:
      options = bo.BrowserOptions()
      options.AppendExtraBrowserArgs([f'{arg}=a', '--another-arg'])
      gpu_integration_test._PreemptArguments(options,
                                             [f'{arg}=b', '--yet-another-arg'])
      self.assertEqual(options.extra_browser_args, set(['--another-arg']))


class GetGPUInfoErrorStringUnittest(unittest.TestCase):

  def testMinimalInformation(self):
    """Tests error string generation w/ the minimum possible information."""
    gpu_info = CreateGpuInfo()
    expected_error = 'primary gpu=device_string'
    self.assertEqual(gpu_integration_test._GetGPUInfoErrorString(gpu_info),
                     expected_error)

  def testGLRenderer(self):
    """Tests error string generation w/ the GL renderer specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'foo',
    })
    expected_error = 'primary gpu=device_string, gl_renderer=foo'
    self.assertEqual(gpu_integration_test._GetGPUInfoErrorString(gpu_info),
                     expected_error)

  def testFeatureStatuses(self):
    """Tests error string generation w/ feature statuses specified."""
    gpu_info = CreateGpuInfo(feature_statuses={
        'featureA': 'on',
        'featureB': 'off',
    })
    expected_error = ('primary gpu=device_string, '
                      'feature_statuses=featureA=on,featureB=off')
    self.assertEqual(gpu_integration_test._GetGPUInfoErrorString(gpu_info),
                     expected_error)

  def testGLRendererAndFeatureStatuses(self):
    """Tests error string generation w/ GL renderer/feature status specified."""
    gpu_info = CreateGpuInfo(aux_attributes={
        'gl_renderer': 'foo',
    },
                             feature_statuses={
                                 'featureA': 'on',
                                 'featureB': 'off'
                             })
    expected_error = ('primary gpu=device_string, '
                      'gl_renderer=foo, '
                      'feature_statuses=featureA=on,featureB=off')
    self.assertEqual(gpu_integration_test._GetGPUInfoErrorString(gpu_info),
                     expected_error)


def _ExtractTestResults(
    test_result: dict[str, dict]) -> tuple[list[str], list[str], list[str]]:
  delimiter = test_result['path_delimiter']
  failures = []
  successes = []
  skips = []

  def _IsLeafNode(node):
    test_dict = node[1]
    return 'expected' in test_dict and isinstance(test_dict['expected'], str)

  node_queues = []
  for t in test_result['tests']:
    node_queues.append((t, test_result['tests'][t]))
  while node_queues:
    node = node_queues.pop()
    full_test_name, test_dict = node
    if _IsLeafNode(node):
      if all(res not in test_dict['expected'].split()
             for res in test_dict['actual'].split()):
        failures.append(full_test_name)
      elif test_dict['expected'] == test_dict['actual'] == 'SKIP':
        skips.append(full_test_name)
      else:
        successes.append(full_test_name)
    else:
      for k in test_dict:
        node_queues.append((f'{full_test_name}{delimiter}{k}', test_dict[k]))
  return successes, failures, skips


if __name__ == '__main__':
  unittest.main(verbosity=2)
