# Copyright 2019 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import inspect
import itertools
import os
import re
import sys
from typing import Iterable, List, Optional, Set, Type
import unittest
import unittest.mock as mock

import gpu_project_config

from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import webgl1_conformance_integration_test as webgl1_cit
from gpu_tests import webgl2_conformance_integration_test as webgl2_cit
from gpu_tests import webgl_test_util
from gpu_tests import webgpu_cts_integration_test

from py_utils import discover
from py_utils import tempfile_ext

from typ import expectations_parser
from typ import json_results

OS_CONDITIONS = ['win', 'mac', 'android']
GPU_CONDITIONS = [
    'amd',
    'arm',
    'broadcom',
    'hisilicon',
    'intel',
    'imagination',
    'nvidia',
    'qualcomm',
    'vivante',
]
WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = [
    'leopard',
    'snowleopard',
    'lion',
    'mountainlion',
    'mavericks',
    'yosemite',
    'sierra',
    'highsierra',
    'mojave',
]
ANDROID_CONDITIONS = [
    'android-lollipop',
    'android-marshmallow',
    'anroid-nougat',
    'android-oreo',
    'android-pie',
    'android-10',
    'android-kitkat',
]
GENERIC_CONDITIONS = OS_CONDITIONS + GPU_CONDITIONS

VALID_BUG_REGEXES = [
    re.compile(r'crbug\.com\/\d+'),
    re.compile(r'crbug\.com\/angleproject\/\d+'),
    re.compile(r'crbug\.com\/dawn\/\d+'),
    re.compile(r'crbug\.com\/swiftshader\/\d+'),
    re.compile(r'crbug\.com\/tint\/\d+'),
    re.compile(r'skbug\.com\/\d+'),
]

_map_specific_to_generic = {sos: 'win' for sos in WIN_CONDITIONS}
_map_specific_to_generic.update({sos: 'mac' for sos in MAC_CONDITIONS})
_map_specific_to_generic.update({sos: 'android' for sos in ANDROID_CONDITIONS})
_map_specific_to_generic['debug-x64'] = 'debug'
_map_specific_to_generic['release-x64'] = 'release'

_get_generic = lambda tags: set(
    _map_specific_to_generic.get(tag, tag) for tag in tags)

ResultType = json_results.ResultType

INTEL_DRIVER_VERSION_SCHEMA = """
The version format of Intel graphics driver is AA.BB.CC.DDDD (legacy schema)
and AA.BB.CCC.DDDD (new schema).

AA.BB: You are free to specify the real number here, but they are meaningless
when comparing two version numbers. Usually it's okay to leave it to "0.0".

CC or CCC: It's meaningful to indicate different branches. Different CC means
different branch, while all CCCs share the same branch.

DDDD: It's always meaningful.
"""


def check_intel_driver_version(version: str) -> bool:
  ver_list = version.split('.')
  if len(ver_list) != 4:
    return False
  for ver in ver_list:
    if not ver.isdigit():
      return False
  return True


def _MapGpuDevicesToVendors(tag_sets: Iterable[Set[str]]) -> None:
  for tag_set in tag_sets:
    if any(gpu in tag_set for gpu in GPU_CONDITIONS):
      _map_specific_to_generic.update({
          t[0]: t[1]
          for t in itertools.permutations(tag_set, 2)
          if t[0].startswith(t[1] + '-')
      })
      break


# No good way to reduce the number of return statements to the required level
# without harming readability.
# pylint: disable=too-many-return-statements,too-many-branches
def _IsDriverTagDuplicated(driver_tag1: str, driver_tag2: str) -> bool:
  if driver_tag1 == driver_tag2:
    return True

  match = gpu_helper.MatchDriverTag(driver_tag1)
  tag1 = (match.group(1), match.group(2), match.group(3))
  match = gpu_helper.MatchDriverTag(driver_tag2)
  tag2 = (match.group(1), match.group(2), match.group(3))
  if not tag1[0] == tag2[0]:
    return False

  operation1, version1 = tag1[1:]
  operation2, version2 = tag2[1:]
  if operation1 == 'ne':
    return not (operation2 == 'eq' and version1 == version2)
  if operation2 == 'ne':
    return not (operation1 == 'eq' and version1 == version2)
  if operation1 == 'eq':
    return gpu_helper.EvaluateVersionComparison(version1, operation2, version2)
  if operation2 == 'eq':
    return gpu_helper.EvaluateVersionComparison(version2, operation1, version1)

  if operation1 in ('ge', 'gt') and operation2 in ('ge', 'gt'):
    return True
  if operation1 in ('le', 'lt') and operation2 in ('le', 'lt'):
    return True

  if operation1 == 'ge':
    if operation2 == 'le':
      return not gpu_helper.EvaluateVersionComparison(version1, 'gt', version2)
    if operation2 == 'lt':
      return not gpu_helper.EvaluateVersionComparison(version1, 'ge', version2)
  if operation1 == 'gt':
    return not gpu_helper.EvaluateVersionComparison(version1, 'ge', version2)
  if operation1 == 'le':
    if operation2 == 'ge':
      return not gpu_helper.EvaluateVersionComparison(version1, 'lt', version2)
    if operation2 == 'gt':
      return not gpu_helper.EvaluateVersionComparison(version1, 'le', version2)
  if operation1 == 'lt':
    return not gpu_helper.EvaluateVersionComparison(version1, 'le', version2)
  assert False
  return False
# pylint: enable=too-many-return-statements,too-many-branches


def _DoTagsConflict(t1: str, t2: str) -> bool:
  if gpu_helper.MatchDriverTag(t1):
    return not _IsDriverTagDuplicated(t1, t2)
  return (t1 != t2 and t1 != _map_specific_to_generic.get(t2, t2)
          and t2 != _map_specific_to_generic.get(t1, t1))


def _ExtractUnitTestTestExpectations(file_name: str) -> List[str]:
  file_name = os.path.join(
      os.path.dirname(os.path.abspath(__file__)), '..', 'unittest_data',
      'test_expectations', file_name)
  test_expectations_list = []
  with open(file_name, 'r') as test_data:
    test_expectations = ''
    reach_end = False
    while not reach_end:
      line = test_data.readline()
      if line:
        line = line.strip()
        if line:
          test_expectations += line + '\n'
          continue
      else:
        reach_end = True

      if test_expectations:
        test_expectations_list.append(test_expectations)
        test_expectations = ''

  assert test_expectations_list
  return test_expectations_list


def CheckTestExpectationsAreForExistingTests(
    unittest_testcase: unittest.TestCase,
    test_class: Type[gpu_integration_test.GpuIntegrationTest],
    mock_options: mock.MagicMock,
    test_names: Optional[List[str]] = None) -> None:
  test_names = test_names or [
      args[0] for args in test_class.GenerateTestCases__RunGpuTest(mock_options)
  ]
  expectations_file = test_class.ExpectationsFiles()[0]
  with open(expectations_file, 'r') as f:
    test_expectations = expectations_parser.TestExpectations()
    test_expectations.parse_tagged_list(f.read(), f.name)
    broke_expectations = '\n'.join([
        "\t- {0}:{1}: Expectation with pattern '{2}' does not match"
        ' any tests in the {3} test suite'.format(f.name, exp.lineno, exp.test,
                                                  test_class.Name())
        for exp in test_expectations.check_for_broken_expectations(test_names)
    ])
    unittest_testcase.assertEqual(
        broke_expectations, '',
        'The following expectations were found to not apply to any tests in '
        'the %s test suite:\n%s' % (test_class.Name(), broke_expectations))


def CheckTestExpectationPatternsForConflicts(expectations: str,
                                             file_name: str) -> str:
  test_expectations = expectations_parser.TestExpectations()
  test_expectations.parse_tagged_list(
      expectations, file_name=file_name, tags_conflict=_DoTagsConflict)
  _MapGpuDevicesToVendors(test_expectations.tag_sets)
  return test_expectations.check_test_expectations_patterns_for_conflicts()


def _FindTestCases() -> List[Type[gpu_integration_test.GpuIntegrationTest]]:
  test_cases = []
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    # Note we deliberately only scan the integration tests as a
    # workaround for http://crbug.com/1195465 .
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=gpu_integration_test.GpuIntegrationTest,
        pattern='*_integration_test.py')
    test_cases.extend(modules_to_classes.values())
  # Filter out WebGPU tests on Windows 7 since listing tests breaks there due
  # to Node not working properly on Windows 7.
  # pylint:disable=no-member
  # pytype: disable=module-attr
  if sys.platform == 'win32' and sys.getwindowsversion().major < 10:
    # pytype: enable=module-attr
    # pylint:enable=no-member
    test_cases = [
        c for c in test_cases
        if c != webgpu_cts_integration_test.WebGpuCtsIntegrationTest
    ]
  return test_cases


class GpuTestExpectationsValidation(unittest.TestCase):
  maxDiff = None

  def testNoConflictsInGpuTestExpectations(self) -> None:
    errors = ''
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        webgl_version = 1
        if issubclass(test_case, webgl2_cit.WebGL2ConformanceIntegrationTest):
          webgl_version = 2
        _ = list(
            test_case.GenerateTestCases__RunGpuTest(
                gpu_helper.GetMockArgs(webgl_version=('%d.0.0' %
                                                      webgl_version))))
        if test_case.ExpectationsFiles():
          with open(test_case.ExpectationsFiles()[0]) as f:
            errors += CheckTestExpectationPatternsForConflicts(
                f.read(), os.path.basename(f.name))
    self.assertEqual(errors, '')

  def testExpectationsFilesCanBeParsed(self) -> None:
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        webgl_version = 1
        if issubclass(test_case, webgl2_cit.WebGL2ConformanceIntegrationTest):
          webgl_version = 2
        _ = list(
            test_case.GenerateTestCases__RunGpuTest(
                gpu_helper.GetMockArgs(webgl_version=('%d.0.0' %
                                                      webgl_version))))
        if test_case.ExpectationsFiles():
          with open(test_case.ExpectationsFiles()[0]) as f:
            test_expectations = expectations_parser.TestExpectations()
            ret, err = test_expectations.parse_tagged_list(f.read(), f.name)
            self.assertEqual(
                ret, 0,
                'Error parsing %s:\n\t%s' % (os.path.basename(f.name), err))

  def testWebglTestPathsExist(self) -> None:
    def _CheckWebglConformanceTestPathIsValid(pattern: str) -> None:
      if not 'WebglExtension_' in pattern:
        full_path = os.path.normpath(
            os.path.join(webgl_test_util.conformance_path, pattern))
        self.assertTrue(os.path.exists(full_path),
                        '%s does not exist' % full_path)

    webgl_test_classes = (
        webgl1_cit.WebGL1ConformanceIntegrationTest,
        webgl2_cit.WebGL2ConformanceIntegrationTest,
    )
    for webgl_version in range(1, 3):
      webgl_test_class = webgl_test_classes[webgl_version - 1]
      _ = list(
          webgl_test_class.GenerateTestCases__RunGpuTest(
              gpu_helper.GetMockArgs(webgl_version='%d.0.0' % webgl_version)))
      with open(webgl_test_class.ExpectationsFiles()[0], 'r') as f:
        expectations = expectations_parser.TestExpectations()
        expectations.parse_tagged_list(f.read())
        for pattern, _ in expectations.individual_exps.items():
          _CheckWebglConformanceTestPathIsValid(pattern)

  def testForBrokenWebglExtensionExpectations(self) -> None:
    webgl_test_classes = (
        webgl1_cit.WebGL1ConformanceIntegrationTest,
        webgl2_cit.WebGL2ConformanceIntegrationTest,
    )
    for webgl_version in range(1, 3):
      webgl_test_class = webgl_test_classes[webgl_version - 1]
      tests = [
          test[0] for test in webgl_test_class.GenerateTestCases__RunGpuTest(
              gpu_helper.GetMockArgs(webgl_version='%d.0.0' % webgl_version))
      ]
      with open(webgl_test_class.ExpectationsFiles()[0], 'r') as f:
        expectations = expectations_parser.TestExpectations()
        expectations.parse_tagged_list(f.read())

        # remove non webgl extension expectations
        for test in list(expectations.individual_exps.keys()):
          if not test.lower().startswith('webglextension'):
            expectations.individual_exps.pop(test)
        for test in list(expectations.glob_exps.keys()):
          if not test.lower().startswith('webglextension'):
            expectations.glob_exps.pop(test)

        broken_expectations = expectations.check_for_broken_expectations(tests)
        msg = ''
        for ununsed_pattern in set(e.test for e in broken_expectations):
          msg += ("Expectations with pattern '{0}' in {1} do not apply to any "
                  'webgl version {2} extension tests\n'.format(
                      ununsed_pattern, os.path.basename(f.name), webgl_version))
        self.assertEqual(msg, '')

  def testForBrokenPixelTestExpectations(self) -> None:
    pixel_test_names = []
    for _, method in inspect.getmembers(
        pixel_test_pages.PixelTestPages, predicate=inspect.isfunction):
      pixel_test_names.extend([
          p.name for p in method(pixel_integration_test.PixelIntegrationTest.
                                 test_base_name)
      ])
    CheckTestExpectationsAreForExistingTests(
        self, pixel_integration_test.PixelIntegrationTest,
        gpu_helper.GetMockArgs(), pixel_test_names)

  def testForBrokenGpuTestExpectations(self) -> None:
    options = gpu_helper.GetMockArgs()
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        if (test_case.Name() not in ('pixel', 'webgl1_conformance',
                                     'webgl2_conformance')
            and test_case.ExpectationsFiles()):
          CheckTestExpectationsAreForExistingTests(self, test_case, options)

  def testExpectationBugValidity(self) -> None:
    expectation_dir = os.path.join(os.path.dirname(__file__),
                                   'test_expectations')
    for expectation_file in (f for f in os.listdir(expectation_dir)
                             if f.endswith('.txt')):
      with open(os.path.join(expectation_dir, expectation_file)) as f:
        content = f.read()
      list_parser = expectations_parser.TaggedTestListParser(content)
      for expectation in list_parser.expectations:
        reason = expectation.reason
        if not reason:
          continue
        if not any(r.match(reason) for r in VALID_BUG_REGEXES):
          self.fail('Bug string "%s" in expectation file %s is either not in a '
                    'recognized format or references an unknown project.' %
                    (reason, expectation_file))

  def testWebglTestExpectationsForDriverTags(self) -> None:
    webgl_test_classes = (
        webgl1_cit.WebGL1ConformanceIntegrationTest,
        webgl2_cit.WebGL2ConformanceIntegrationTest,
    )
    expectations_driver_tags = set()
    for webgl_version in range(1, 3):
      webgl_conformance_test_class = webgl_test_classes[webgl_version - 1]
      _ = list(
          webgl_conformance_test_class.GenerateTestCases__RunGpuTest(
              gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % webgl_version))))
      with open(webgl_conformance_test_class.ExpectationsFiles()[0], 'r') as f:
        parser = expectations_parser.TestExpectations()
        parser.parse_tagged_list(f.read(), f.name)
        driver_tag_set = set()
        for tag_set in parser.tag_sets:
          if gpu_helper.MatchDriverTag(list(tag_set)[0]):
            for tag in tag_set:
              match = gpu_helper.MatchDriverTag(tag)
              self.assertIsNotNone(match)
              if match.group(1) == 'intel':
                self.assertTrue(check_intel_driver_version(match.group(3)))

            self.assertSetEqual(driver_tag_set, set())
            driver_tag_set = tag_set
          else:
            for tag in tag_set:
              self.assertIsNone(gpu_helper.MatchDriverTag(tag))
        expectations_driver_tags |= driver_tag_set

    self.assertEqual(gpu_helper.ExpectationsDriverTags(),
                     expectations_driver_tags)


class TestGpuTestExpectationsValidators(unittest.TestCase):
  def testConflictInTestExpectationsWithGpuDriverTags(self) -> None:
    failed_test_expectations = _ExtractUnitTestTestExpectations(
        'failed_test_expectations_with_driver_tags.txt')
    self.assertTrue(
        all(
            CheckTestExpectationPatternsForConflicts(test_expectations,
                                                     'test.txt')
            for test_expectations in failed_test_expectations))

  def testNoConflictInTestExpectationsWithGpuDriverTags(self) -> None:
    passed_test_expectations = _ExtractUnitTestTestExpectations(
        'passed_test_expectations_with_driver_tags.txt')
    for test_expectations in passed_test_expectations:
      errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                        'test.txt')
      self.assertFalse(errors)

  def testConflictsBetweenAngleAndNonAngleConfigurations(self) -> None:
    test_expectations = """
    # tags: [ android ]
    # tags: [ android-nexus-5x ]
    # tags: [ opengles ]
    # results: [ RetryOnFailure Skip ]
    [ android android-nexus-5x ] a/b/c/d [ RetryOnFailure ]
    [ android opengles ] a/b/c/d [ Skip ]
    """
    errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                      'test.txt')
    self.assertTrue(errors)

  def testConflictBetweenTestExpectationsWithOsNameAndOSVersionTags(self
                                                                    ) -> None:
    test_expectations = """# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel xp ] a/b/c/d [ Failure ]
    [ intel win debug ] a/b/c/d [ Skip ]
    """
    errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                      'test.txt')
    self.assertTrue(errors)

  def testNoConflictBetweenOsVersionTags(self) -> None:
    test_expectations = """# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel win7 ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    """
    errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                      'test.txt')
    self.assertFalse(errors)

  def testConflictBetweenGpuVendorAndGpuDeviceIdTags(self) -> None:
    test_expectations = """# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 ] a/b/c/d [ Failure ]
    [ nvidia debug ] a/b/c/d [ Skip ]
    """
    errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                      'test.txt')
    self.assertTrue(errors)

  def testNoConflictBetweenGpuDeviceIdTags(self) -> None:
    test_expectations = """# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 win7 ] a/b/c/d [ Failure ]
    [ nvidia-0x02 win7 debug ] a/b/c/d [ Skip ]
    [ nvidia win debug ] a/b/c/* [ Skip ]
    """
    errors = CheckTestExpectationPatternsForConflicts(test_expectations,
                                                      'test.txt')
    self.assertFalse(errors)

  def testFoundBrokenExpectations(self) -> None:
    test_expectations = ('# tags: [ mac ]\n'
                         '# results: [ Failure ]\n'
                         '[ mac ] a/b/d [ Failure ]\n'
                         'a/c/* [ Failure ]\n')
    options = gpu_helper.GetMockArgs()
    test_class = gpu_integration_test.GpuIntegrationTest
    with tempfile_ext.NamedTemporaryFile(mode='w') as expectations_file,    \
         mock.patch.object(
             test_class, 'GenerateTestCases__RunGpuTest',
             return_value=[('a/b/c', ())]),                                 \
         mock.patch.object(
             test_class,
             'ExpectationsFiles', return_value=[expectations_file.name]):
      expectations_file.write(test_expectations)
      expectations_file.close()
      with self.assertRaises(AssertionError) as context:
        CheckTestExpectationsAreForExistingTests(self, test_class, options)
      self.assertIn(
          'The following expectations were found to not apply'
          ' to any tests in the GpuIntegrationTest test suite',
          str(context.exception))
      self.assertIn(
          "4: Expectation with pattern 'a/c/*' does not match"
          ' any tests in the GpuIntegrationTest test suite',
          str(context.exception))
      self.assertIn(
          "3: Expectation with pattern 'a/b/d' does not match"
          ' any tests in the GpuIntegrationTest test suite',
          str(context.exception))


  def testDriverVersionComparision(self) -> None:
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'eq',
                                             '24.20.100.7000'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100', 'ne',
                                             '24.20.100.7000'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'gt',
                                             '24.20.100'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000a', 'gt',
                                             '24.20.100.7000'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'lt',
                                             '24.20.100.7001'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'lt',
                                             '24.20.200.6000'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'lt',
                                             '25.30.100.6000', 'linux',
                                             'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'gt',
                                             '25.30.100.6000', 'win', 'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.101.6000', 'gt',
                                             '25.30.100.7000', 'win', 'intel'))
    self.assertFalse(
        gpu_helper.EvaluateVersionComparison('24.20.99.7000', 'gt',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.99.7000', 'lt',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.99.7000', 'ne',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertFalse(
        gpu_helper.EvaluateVersionComparison('24.20.100', 'lt',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertFalse(
        gpu_helper.EvaluateVersionComparison('24.20.100', 'gt',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100', 'ne',
                                             '24.20.100.7000', 'win', 'intel'))
    self.assertTrue(
        gpu_helper.EvaluateVersionComparison('24.20.100.7000', 'eq',
                                             '25.20.100.7000', 'win', 'intel'))


if __name__ == '__main__':
  unittest.main(verbosity=2)
