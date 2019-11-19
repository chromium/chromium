# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gpu_project_config
import inspect
import itertools
import mock
import os
import tempfile
import unittest

from gpu_tests import gpu_helper
from gpu_tests import gpu_integration_test
from gpu_tests import pixel_integration_test
from gpu_tests import pixel_test_pages
from gpu_tests import webgl_conformance_integration_test
from gpu_tests import webgl_test_util

from py_utils import discover

from typ import expectations_parser
from typ import json_results

OS_CONDITIONS = ['win', 'mac', 'android']
GPU_CONDITIONS = ['amd', 'arm', 'broadcom', 'hisilicon', 'intel', 'imagination',
                  'nvidia', 'qualcomm', 'vivante']
WIN_CONDITIONS = ['xp', 'vista', 'win7', 'win8', 'win10']
MAC_CONDITIONS = ['leopard', 'snowleopard', 'lion', 'mountainlion',
                  'mavericks', 'yosemite', 'sierra', 'highsierra', 'mojave']
ANDROID_CONDITIONS = ['android-lollipop', 'android-marshmallow',
                      'anroid-nougat', 'android-oreo', 'android-pie',
                      'android-10', 'android-kitkat']
GENERIC_CONDITIONS = OS_CONDITIONS + GPU_CONDITIONS

_map_specific_to_generic = {sos:'win' for sos in WIN_CONDITIONS}
_map_specific_to_generic.update({sos:'mac' for sos in MAC_CONDITIONS})
_map_specific_to_generic.update({sos:'android' for sos in ANDROID_CONDITIONS})

_get_generic = lambda tags: set(
    [_map_specific_to_generic.get(tag, tag) for tag in tags])

ResultType = json_results.ResultType


def _MapGpuDevicesToVendors(tag_sets):
  for tag_set in tag_sets:
    if any(gpu in tag_set for gpu in GPU_CONDITIONS):
      _map_specific_to_generic.update(
          {t[0]: t[1] for t in
           itertools.permutations(tag_set, 2) if t[0].startswith(t[1] + '-')})
      break


def _IsDriverTagDuplicated(driver_tag1, driver_tag2):
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
  elif operation2 == 'ne':
    return not (operation1 == 'eq' and version1 == version2)
  elif operation1 == 'eq':
    return gpu_helper.EvaluateVersionComparison(
        version1, operation2, version2)
  elif operation2 == 'eq':
    return gpu_helper.EvaluateVersionComparison(
        version2, operation1, version1)

  if operation1 == 'ge' or operation1 == 'gt':
    if operation2 == 'ge' or operation2 == 'gt':
      return True
  elif operation1 == 'le' or operation1 == 'lt':
    if operation2 == 'le' or operation2 == 'lt':
      return True

  if operation1 == 'ge':
    if operation2 == 'le':
      return not gpu_helper.EvaluateVersionComparison(version1, 'gt', version2)
    elif operation2 == 'lt':
      return not gpu_helper.EvaluateVersionComparison(version1, 'ge', version2)
  elif operation1 == 'gt':
    return not gpu_helper.EvaluateVersionComparison(version1, 'ge', version2)
  elif operation1 == 'le':
    if operation2 == 'ge':
      return not gpu_helper.EvaluateVersionComparison(version1, 'lt', version2)
    elif operation2 == 'gt':
      return not gpu_helper.EvaluateVersionComparison(version1, 'le', version2)
  elif operation1 == 'lt':
    return not gpu_helper.EvaluateVersionComparison(version1, 'le', version2)
  else:
    assert False


def _DoTagsConflict(t1, t2):
  if gpu_helper.MatchDriverTag(t1):
    return not _IsDriverTagDuplicated(t1, t2)
  else:
    return (t1 != t2 and t1 != _map_specific_to_generic.get(t2, t2)
            and t2 != _map_specific_to_generic.get(t1, t1))


def _ExtractUnitTestTestExpectations(file_name):
  file_name = os.path.join(os.path.dirname(os.path.abspath(__file__)),
      '..', 'unittest_data', 'test_expectations', file_name)
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
    test_class, mock_options, test_names=None):
  test_names = test_names or [
      args[0] for args in
      test_class.GenerateGpuTests(mock_options)]
  expectations_file = test_class.ExpectationsFiles()[0]
  with open(expectations_file, 'r') as f:
    test_expectations = expectations_parser.TestExpectations()
    test_expectations.parse_tagged_list(f.read(), f.name)
    broke_expectations = '\n'.join(
        ["\t- {0}:{1}: Expectation with pattern '{2}' does not match"
         " any tests in the {3} test suite".format(
             f.name, exp.lineno, exp.test, test_class.Name())
         for exp in test_expectations.check_for_broken_expectations(
            test_names)])
    if broke_expectations:
      broke_expectations = (
          'The following expectations were found to'
          ' not apply to any tests in the %s test suite\n' % (test_class.Name())
          + broke_expectations)
      assert not broke_expectations, broke_expectations


def CheckTestExpectationPatternsForConflicts(expectations, file_name):
  test_expectations = expectations_parser.TestExpectations()
  test_expectations.parse_tagged_list(
      expectations, file_name=file_name, tags_conflict=_DoTagsConflict)
  _MapGpuDevicesToVendors(test_expectations.tag_sets)
  return test_expectations.check_test_expectations_patterns_for_conflicts()


def _CheckWebglConformanceTestPathIsValid(pattern):
  if not 'WebglExtension_' in pattern:
    full_path = os.path.normpath(os.path.join(
        webgl_test_util.conformance_path, pattern))
    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified in ' +
        'expectation does not exist: ' + full_path)


def _FindTestCases():
  test_cases = []
  for start_dir in gpu_project_config.CONFIG.start_dirs:
    modules_to_classes = discover.DiscoverClasses(
        start_dir,
        gpu_project_config.CONFIG.top_level_dir,
        base_class=gpu_integration_test.GpuIntegrationTest)
    test_cases.extend(modules_to_classes.values())
  return test_cases


class GpuTestExpectationsValidation(unittest.TestCase):
  def testNoConflictsInGpuTestExpectations(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    errors = ''
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for webgl_version in xrange(
            1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(test_case.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % webgl_version))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              errors += CheckTestExpectationPatternsForConflicts(f.read(),
                os.path.basename(f.name))
    assert not errors, errors

  def testExpectationsFilesCanBeParsed(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        for webgl_version in xrange(
            1, 2 + (test_case == webgl_conformance_test_class)):
          _ = list(
              test_case.GenerateGpuTests(
                  gpu_helper.GetMockArgs(
                      webgl_version=('%d.0.0' % webgl_version))))
          if test_case.ExpectationsFiles():
            with open(test_case.ExpectationsFiles()[0]) as f:
              test_expectations = expectations_parser.TestExpectations()
              ret, err = test_expectations.parse_tagged_list(f.read(), f.name)
              assert not ret, (
                  'There was an error parsing %s, the error is:\n\t%s' %
                  (os.path.basename(f.name), err))

  def testWebglTestPathsExist(self):
    webgl_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for webgl_version in xrange(1, 3):
      _ = list(
          webgl_test_class.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version='%d.0.0' % webgl_version)))
      with open(webgl_test_class.ExpectationsFiles()[0], 'r') as f:
        expectations = expectations_parser.TestExpectations()
        expectations.parse_tagged_list(f.read())
        for pattern, _ in expectations.individual_exps.items():
          _CheckWebglConformanceTestPathIsValid(pattern)

  def testForBrokenWebglExtensionExpectations(self):
    webgl_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    for webgl_version in xrange(1, 3):
      tests = [
          test[0] for test in
          webgl_test_class.GenerateGpuTests(
              gpu_helper.GetMockArgs(webgl_version='%d.0.0' % webgl_version))]
      with open(webgl_test_class.ExpectationsFiles()[0], 'r') as f:
        expectations = expectations_parser.TestExpectations()
        expectations.parse_tagged_list(f.read())
        patterns_to_exps = expectations.individual_exps.copy()
        patterns_to_exps.update(expectations.glob_exps)
        patterns_to_exps = {k: v for k, v in patterns_to_exps.items()
                            if k.lower().startswith('webglextension')}
        broken_expectations = expectations.get_broken_expectations(
            patterns_to_exps, tests)
        msg = ''
        for ununsed_pattern in set([e.test for e in  broken_expectations]):
          msg += ("Expectations with pattern '{0}' in {1} do not apply to any "
                  "webgl version {2} extension tests\n".format(
                      ununsed_pattern, os.path.basename(f.name), webgl_version))
        assert not msg, msg

  def testForBrokenPixelTestExpectations(self):
    pixel_test_names = []
    for _, method in inspect.getmembers(
        pixel_test_pages.PixelTestPages, predicate=inspect.isfunction):
      pixel_test_names.extend(
          [p.name for p in method(
              pixel_integration_test.PixelIntegrationTest.test_base_name)])
    CheckTestExpectationsAreForExistingTests(
        pixel_integration_test.PixelIntegrationTest,
        gpu_helper.GetMockArgs(), pixel_test_names)

  def testForBrokenGpuTestExpectations(self):
    options = gpu_helper.GetMockArgs()
    for test_case in _FindTestCases():
      if 'gpu_tests.gpu_integration_test_unittest' not in test_case.__module__:
        if (test_case.Name() not in ('pixel', 'webgl_conformance')
            and test_case.ExpectationsFiles()):
          CheckTestExpectationsAreForExistingTests(test_case, options)

  def testWebglTestExpectationsForDriverTags(self):
    webgl_conformance_test_class = (
        webgl_conformance_integration_test.WebGLConformanceIntegrationTest)
    expectations_driver_tags = set()
    for webgl_version in range(1, 3):
      _ = list(webgl_conformance_test_class.GenerateGpuTests(
          gpu_helper.GetMockArgs(webgl_version=('%d.0.0' % webgl_version))))
      with open(webgl_conformance_test_class.ExpectationsFiles()[0], 'r') as f:
        parser = expectations_parser.TestExpectations()
        parser.parse_tagged_list(f.read(), f.name)
        driver_tag_set = set()
        for tag_set in parser.tag_sets:
          if gpu_helper.MatchDriverTag(list(tag_set)[0]):
            for tag in tag_set:
              assert gpu_helper.MatchDriverTag(tag)
            assert not driver_tag_set
            driver_tag_set = tag_set
          else:
            for tag in tag_set:
              assert not gpu_helper.MatchDriverTag(tag)
        expectations_driver_tags |= driver_tag_set

    self.assertEqual(
        gpu_helper.ExpectationsDriverTags(),
        expectations_driver_tags)


class TestGpuTestExpectationsValidators(unittest.TestCase):
  def testConflictInTestExpectationsWithGpuDriverTags(self):
    failed_test_expectations = _ExtractUnitTestTestExpectations(
        'failed_test_expectations_with_driver_tags.txt')
    self.assertTrue(
        all(CheckTestExpectationPatternsForConflicts(
                test_expectations, 'test.txt')
        for test_expectations in failed_test_expectations))

  def testNoConflictInTestExpectationsWithGpuDriverTags(self):
    passed_test_expectations = _ExtractUnitTestTestExpectations(
        'passed_test_expectations_with_driver_tags.txt')
    for test_expectations in passed_test_expectations:
      errors = CheckTestExpectationPatternsForConflicts(
          test_expectations, 'test.txt')
      self.assertFalse(errors)

  def testConflictsBetweenAngleAndNonAngleConfigurations(self):
    test_expectations = '''
    # tags: [ android ]
    # tags: [ qualcomm-adreno-(tm)-418 ]
    # tags: [ opengles ]
    # results: [ RetryOnFailure Skip ]
    [ android qualcomm-adreno-(tm)-418 ] a/b/c/d [ RetryOnFailure ]
    [ android opengles ] a/b/c/d [ Skip ]
    '''
    errors = CheckTestExpectationPatternsForConflicts(
        test_expectations, 'test.txt')
    self.assertTrue(errors)

  def testConflictBetweenTestExpectationsWithOsNameAndOSVersionTags(self):
    test_expectations = '''# tags: [ mac win linux xp ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel xp ] a/b/c/d [ Failure ]
    [ intel win debug ] a/b/c/d [ Skip ]
    '''
    errors = CheckTestExpectationPatternsForConflicts(
        test_expectations, 'test.txt')
    self.assertTrue(errors)

  def testNoConflictBetweenOsVersionTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ intel win7 ] a/b/c/d [ Failure ]
    [ intel xp debug ] a/b/c/d [ Skip ]
    '''
    errors = CheckTestExpectationPatternsForConflicts(
        test_expectations, 'test.txt')
    self.assertFalse(errors)

  def testConflictBetweenGpuVendorAndGpuDeviceIdTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 ] a/b/c/d [ Failure ]
    [ nvidia debug ] a/b/c/d [ Skip ]
    '''
    errors = CheckTestExpectationPatternsForConflicts(
        test_expectations, 'test.txt')
    self.assertTrue(errors)

  def testNoConflictBetweenGpuDeviceIdTags(self):
    test_expectations = '''# tags: [ mac win linux xp win7 ]
    # tags: [ intel amd nvidia nvidia-0x01 nvidia-0x02 ]
    # tags: [ debug release ]
    # results: [ Failure Skip ]
    [ nvidia-0x01 win7 ] a/b/c/d [ Failure ]
    [ nvidia-0x02 win7 debug ] a/b/c/d [ Skip ]
    [ nvidia win debug ] a/b/c/* [ Skip ]
    '''
    errors = CheckTestExpectationPatternsForConflicts(
        test_expectations, 'test.txt')
    self.assertFalse(errors)

  def testFoundBrokenExpectations(self):
    test_expectations = (
        '# tags: [ mac ]\n'
        '# results: [ Failure ]\n'
        '[ mac ] a/b/d [ Failure ]\n'
        'a/c/* [ Failure ]\n')
    options = gpu_helper.GetMockArgs()
    expectations_file = tempfile.NamedTemporaryFile(delete=False)
    expectations_file.write(test_expectations)
    expectations_file.close()
    test_class = gpu_integration_test.GpuIntegrationTest
    with mock.patch.object(
        test_class, 'GenerateGpuTests', return_value=[('a/b/c', ())]):
      with mock.patch.object(
          test_class, 'ExpectationsFiles',
          return_value=[expectations_file.name]):
        with self.assertRaises(AssertionError) as context:
          CheckTestExpectationsAreForExistingTests(test_class, options)
        self.assertIn('The following expectations were found to not apply'
                      ' to any tests in the GpuIntegrationTest test suite',
                      str(context.exception))
        self.assertIn('4: Expectation with pattern \'a/c/*\' does not match'
                      ' any tests in the GpuIntegrationTest test suite',
                      str(context.exception))
        self.assertIn('3: Expectation with pattern \'a/b/d\' does not match'
                      ' any tests in the GpuIntegrationTest test suite',
                      str(context.exception))
