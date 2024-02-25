# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
import re
import subprocess
import sys

import test_runner_errors

LOGGER = logging.getLogger(__name__)

# WARNING: THESE DUPLICATE CONSTANTS IN:
# //build/scripts/slave/recipe_modules/ios/api.py

# Regex to parse all compiled EG tests, including disabled (prepended with
# DISABLED_ or FLAKY_).
TEST_NAMES_DEBUG_APP_PATTERN = re.compile(
    'imp .*-\[(?P<testSuite>[A-Z][A-Za-z0-9_]'
    '*Test[Case]*) (?P<testMethod>(?:DISABLED_|FLAKY_)?test[A-Za-z0-9_]*)\]')
TEST_CLASS_RELEASE_APP_PATTERN = re.compile(
    r'name +0[xX]\w+ '
    '(?P<testSuite>[A-Z][A-Za-z0-9_]*Test(?:Case|))\n')
# Regex to parse all compiled EG tests, including disabled (prepended with
# DISABLED_ or FLAKY_).
TEST_NAME_RELEASE_APP_PATTERN = re.compile(
    r'name +0[xX].+ (?P<testCase>(?:DISABLED_|FLAKY_)?test[A-Za-z0-9_]+)\n')
# 'ChromeTestCase' and 'BaseEarlGreyTestCase' are parent classes
# of all EarlGrey/EarlGrey2 test classes. 'appConfigurationForTestCase' is a
# class method. They have no real tests.
IGNORED_CLASSES = [
    'BaseEarlGreyTestCase', 'ChromeTestCase', 'appConfigurationForTestCase',
    'setUpForTestCase', 'GREYTest'
]
# Test class can inherit from another class so the parent class's test methods
# will automatically be run in the child class. However, the existing otool
# parsing logic does not take into account of inheritance.
# The below dictionary has subclass as the key and superclass as value.
INHERITANCE_CLASS_DICT = {
    'PasswordManagerPasswordCheckupDisabledTestCase': 'PasswordManagerTestCase'
}


class OtoolError(test_runner_errors.Error):
  """OTool non-zero error code"""

  def __init__(self, code):
    super(OtoolError,
          self).__init__('otool returned a non-zero return code: %s' % code)


class ShardingError(test_runner_errors.Error):
  """Error related with sharding logic."""
  pass


def shard_index():
  """Returns shard index in environment, or 0 if not in sharding environment."""
  return int(os.getenv('GTEST_SHARD_INDEX', 0))


def total_shards():
  """Returns total shard count in environment, or 1 if not in environment."""
  return int(os.getenv('GTEST_TOTAL_SHARDS', 1))


def determine_app_path(app, host_app=None, release=False):
  """String manipulate args.app and args.host to determine what path to use
    for otools

    Args:
        app: (string) args.app
        host_app: (string) args.host_app
        release: (bool) whether it's a release app

    Returns:
        (string) path to app for otools to analyze
    """
  # run.py invoked via ../../ios/build/bots/scripts/, so we reverse this
  dirname = os.path.dirname(os.path.abspath(__file__))

  build_type = "Release" if release else "Debug"
  # location of app: /b/s/w/ir/out/{build_type}/test.app
  full_app_path = os.path.normpath(
      os.path.join(dirname, '../../../..', 'out', build_type, app))

  # ie/ if app_path = "../../some.app", app_name = some
  app_name = os.path.basename(app)
  app_name = app_name[:app_name.rindex('.app')]

  # Default app_path looks like /b/s/w/ir/out/{build_type}/test.app/test
  app_path = os.path.join(full_app_path, app_name)

  if host_app and host_app != 'NO_PATH':
    LOGGER.debug("Detected EG2 test while building application path. "
                 "Host app: {}".format(host_app))
    # EG2 tests always end in -Runner, so we split that off
    app_name = app_name[:app_name.rindex('-Runner')]
    app_path = os.path.join(full_app_path, 'PlugIns',
                            '{}.xctest'.format(app_name), app_name)

  return app_path


def _execute(cmd):
  """Helper for executing a command."""
  LOGGER.info('otool command: {}'.format(cmd))
  process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
  stdout = process.communicate()[0]

  retcode = process.returncode
  LOGGER.info('otool return status code: {}'.format(retcode))
  if retcode:
    raise OtoolError(retcode)

  return stdout


def fetch_test_names_for_release(stdout):
  """Parse otool output to get all testMethods in all TestCases in the
     format of (TestCase, testMethod) including disabled tests, in release app.

     WARNING: This logic is similar to what's found in
      //build/scripts/slave/recipe_modules/ios/api.py

    Args:
        stdout: (bytes) response of 'otool -ov'

    Returns:
        (list) a list of (TestCase, testMethod), containing disabled tests.
    """
  # For Release builds `otool -ov` command generates output that is
  # different from Debug builds.
  # Parsing implemented in such a way:
  # 1. Parse test class names.
  # 2. If they are not in ignored list, parse test method names.
  # 3. Calculate test count per test class.
  # |stdout| will be bytes on python3, and therefore must be decoded prior
  # to running a regex.
  if sys.version_info.major == 3:
    stdout = stdout.decode('utf-8')
  res = re.split(TEST_CLASS_RELEASE_APP_PATTERN, stdout)
  # Ignore 1st element in split since it does not have any test class data
  test_classes_output = res[1:]
  test_names = []
  output_size = len(test_classes_output)
  # TEST_CLASS_RELEASE_APP_PATTERN appears twice for each TestCase. First time
  # is for class definition followed by instance methods. Second time is for
  # meta class definition followed by class methods. Lines in between the two
  # contain testMethods for it. Thus, index 0, 4, 8... are test class names.
  # Index 1, 5, 9... are outputs including corresponding test methods.
  for group_index in range(output_size // 4):
    class_index = group_index * 4
    if (class_index + 2 >= output_size or test_classes_output[class_index] !=
        test_classes_output[class_index + 2]):
      raise ShardingError('Incorrect otool output in which a test class name '
                          'doesn\'t appear in group of 2. Test class: %s' %
                          test_classes_output[class_index])

    test_class = test_classes_output[class_index]
    if test_class in IGNORED_CLASSES:
      continue

    class_output = test_classes_output[class_index + 1]
    methods = TEST_NAME_RELEASE_APP_PATTERN.findall(class_output)
    test_names.extend((test_class, test_method) for test_method in methods)
  return test_names


def fetch_test_names_for_debug(stdout):
  """Parse otool output to get all testMethods in all TestCases in the
     format of (TestCase, testMethod) including disabled tests, in debug app.

    Args:
        stdout: (bytes) response of 'otool -ov'

    Returns:
        (list) a list of (TestCase, testMethod), containing disabled tests.
    """
  # |stdout| will be bytes on python3, and therefore must be decoded prior
  # to running a regex.
  if sys.version_info.major == 3:
    stdout = stdout.decode('utf-8')
  test_names = TEST_NAMES_DEBUG_APP_PATTERN.findall(stdout)
  test_names = list(
      map(lambda test_name: (test_name[0], test_name[1]), test_names))
  return list(
      filter(lambda test_name: test_name[0] not in IGNORED_CLASSES, test_names))


def fetch_test_names(app, host_app, release, enabled_tests_only=True):
  """Determine the list of (TestCase, testMethod) for the app.

    Args:
        app: (string) path to app
        host_app: (string) path to host app. None or "NO_PATH" for EG1.
        release: (bool) whether this is a release build.
        enabled_tests_only: (bool) output only enabled tests.

    Returns:
        (list) a list of (TestCase, testMethod).
    """
  # Determine what path to use
  app_path = determine_app_path(app, host_app, release)

  # Use otools to get the test counts
  cmd = ['otool', '-ov', app_path]
  stdout = _execute(cmd)
  LOGGER.info("Ignored test classes: {}".format(IGNORED_CLASSES))
  if release:
    LOGGER.info("Release build detected. Fetching test names for release.")
  all_test_names = (
      fetch_test_names_for_release(stdout)
      if release else fetch_test_names_for_debug(stdout))
  enabled_test_names = (
      list(
          filter(lambda test_name: test_name[1].startswith('test'),
                 all_test_names)))
  return enabled_test_names if enabled_tests_only else all_test_names


def balance_into_sublists(test_counts, total_shards):
  """Augment the result of otool into balanced sublists

  Args:
    test_counts: (collections.Counter) dict of test_case to test case numbers
    total_shards: (int) total number of shards this was divided into

  Returns:
    list of list of test classes
  """

  class Shard(object):
    """Stores list of test classes and number of all tests"""

    def __init__(self):
      self.test_classes = []
      self.size = 0

  shards = [Shard() for i in range(total_shards)]

  # TODO(crbug.com/1480192): we should implement a programmatic logic
  # to detect inheritance instead of hardcoding them manually.
  # It's very challenging to use regex to detect inheritance in otool,
  # so it be might best to dig around xcodebuild -enumerate-tests.
  for subclass, superclass in INHERITANCE_CLASS_DICT.items():
    if superclass in test_counts and subclass in test_counts:
      test_counts[subclass] += test_counts[superclass]

  # Balances test classes between shards to have
  # approximately equal number of tests per shard.
  for test_class, number_of_test_methods in test_counts.most_common():
    min_shard = min(shards, key=lambda shard: shard.size)
    min_shard.test_classes.append(test_class)
    min_shard.size += number_of_test_methods
    LOGGER.debug('%s test case is allocated to shard %s with %s test methods' %
                 (test_class, shards.index(min_shard), number_of_test_methods))

  sublists = [shard.test_classes for shard in shards]
  return sublists


def shard_test_cases(args, shard_index, total_shards):
  """Shard test cases into total_shards, and determine which test cases to
    run for this shard.

    Args:
        args: all parsed arguments passed to run.py
        shard_index: the shard index(number) for this run
        total_shards: the total number of shards for this test

    Returns: a list of test cases to execute
    """
  # Convert to dict format
  dict_args = vars(args)
  app = dict_args['app']
  host_app = dict_args.get('host_app', None)
  release = dict_args.get('release', False)

  test_counts = collections.Counter(
      test_class for test_class, _ in fetch_test_names(app, host_app, release))

  # Ensure shard and total shard is int
  shard_index = int(shard_index)
  total_shards = int(total_shards)

  sublists = balance_into_sublists(test_counts, total_shards)
  tests = sublists[shard_index]

  LOGGER.info("Tests to be executed this round: {}".format(tests))
  return tests
