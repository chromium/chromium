# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test apps for running tests using xcodebuild."""

import os
import platform
import plistlib
import struct
import subprocess
import time
from typing import Tuple, List

import shard_util
import test_runner
import test_runner_errors
import xcode_util

# Including this test arg will have the gTest launcher generate
# an info file containing all the compiled tests for this test run
# This should be on by default
GENERATE_COMPILED_GTESTS_FILE_TEST_ARG = (
    '--write-compiled-tests-json-to-writable-path')


def get_gtest_filter(included, excluded):
  """Returns the GTest filter to filter the given test cases.

  If only included or excluded is provided, uses GTest filter inclusion or
  exclusion syntax for the given list. If both are provided, uses included list
  minus any tests in excluded list as tests to be included.

  Args:
    included: List of test cases to be included.
    excluded: List of test cases to be excluded.

  Returns:
    A string which can be supplied to --gtest_filter.
  """
  assert included or excluded, 'One of included or excluded list should exist.'
  if included and excluded:
    included = list(set(included) - set(excluded))
    excluded = []
  # A colon-separated list of tests cases.
  # e.g. a:b:c matches a, b, c.
  # e.g. -a:b:c matches everything except a, b, c.
  test_filter = ':'.join(test for test in sorted(included + excluded))

  # This means all tests in |included| are in |excluded|.
  if not test_filter:
    return '-*'
  return '-%s' % test_filter if excluded else test_filter


def get_bundle_id(app_path):
  """Get bundle identifier for app.

  Args:
    app_path: (str) A path to app.
  """
  return subprocess.check_output([
      '/usr/libexec/PlistBuddy',
      '-c',
      'Print:CFBundleIdentifier',
      os.path.join(app_path, 'Info.plist'),
  ]).decode("utf-8").rstrip()


def is_running_rosetta():
  """Returns whether Python is being translated by Rosetta.

  Returns:
    True if the Python interpreter is being run as an x86_64 binary on an arm64
    macOS machine. False if it is running as an arm64 binary, or if it is
    running on an Intel machine.
  """
  if platform.system() == 'Darwin':
    translated = subprocess.check_output(
        ['sysctl', '-i', '-b', 'sysctl.proc_translated'])
    # "sysctl -b" is expected to return a 4-byte integer response. 1 means the
    # current process is running under Rosetta, 0 means it is not. On x86_64
    # machines, this variable does not exist at all, so "-i" is used to return a
    # 0-byte response instead of throwing an error.
    if len(translated) != 4:
      return False
    return struct.unpack('i', translated)[0] > 0
  return False

class GTestsApp(object):
  """Gtests app to run.

  Stores data about egtests:
    test_app: full path to an app.
  """

  def __init__(self, test_app, **kwargs):
    """Initialize Egtests.

    Args:
      test_app: (str) full path to egtests app.
      (Following are potential args in **kwargs)
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.
      release: (bool) Whether the app is release build.
      repeat_count: (int) Number of times to run each test case.
      inserted_libs: List of libraries to insert when running the test.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    if not os.path.exists(test_app):
      raise test_runner.AppNotFoundError(test_app)
    self.test_app_path = test_app
    self.project_path = os.path.dirname(self.test_app_path)
    self.test_args = kwargs.get('test_args') or []
    self.env_vars = {}
    for env_var in kwargs.get('env_vars') or []:
      env_var = env_var.split('=', 1)
      self.env_vars[env_var[0]] = None if len(env_var) == 1 else env_var[1]
    # Keep the initial included tests since creating target. Do not modify.
    self.initial_included_tests = kwargs.get('included_tests') or []
    # This may be modified between test launches.
    self.included_tests = kwargs.get('included_tests') or []
    # This may be modified between test launches.
    self.excluded_tests = kwargs.get('excluded_tests') or []
    self.disabled_tests = []
    self.module_name = os.path.splitext(os.path.basename(test_app))[0]
    self.release = kwargs.get('release')
    self.repeat_count = kwargs.get('repeat_count') or 1
    self.host_app_path = kwargs.get('host_app_path')
    self.inserted_libs = kwargs.get('inserted_libs') or []

  def _additional_inserted_libs(self):
    """Returns additional libraries to add to inserted_libs."""
    return []

  def remove_gtest_sharding_env_vars(self):
    """Removes sharding related env vars from self.env_vars."""
    for env_var_key in ['GTEST_SHARD_INDEX', 'GTEST_TOTAL_SHARDS']:
      self.env_vars.pop(env_var_key, None)

  def fill_xctest_run(self, out_dir):
    """Fills xctestrun file by egtests.

    Args:
      out_dir: (str) A path where xctestrun will store.

    Returns:
      A path to xctestrun file.
    """
    folder = os.path.abspath(os.path.join(out_dir, os.pardir))
    if not os.path.exists(folder):
      os.makedirs(folder)
    xctestrun = os.path.join(folder, 'run_%d.xctestrun' % int(time.time()))
    # Creates a dict with data about egtests to run - fill all required fields:
    # egtests_module, egtest_app_path, egtests_xctest_path and
    # filtered tests if filter is specified.
    # Write data in temp xctest run file.
    with open(xctestrun, "wb") as f:
      plistlib.dump(self.fill_xctestrun_node(), f)
    return xctestrun

  @staticmethod
  def _replace_multiple_slashes(name):
    """Replace slashes with dots (.) except at the end."""
    count = name.count('/')
    if count == 0:
      return name
    return name.replace('/', '.', count - 1)

  def fill_xctestrun_node(self):
    """Fills only required nodes for egtests in xctestrun file.

    Returns:
      A node with filled required fields about egtests.
    """
    module = self.module_name + '_module'

    # If --run-with-custom-webkit is passed as a test arg, set up
    # DYLD_FRAMEWORK_PATH and DYLD_LIBRARY_PATH to load the custom webkit
    # modules.
    dyld_path = self.project_path
    if '--run-with-custom-webkit' in self.test_args:
      if self.host_app_path:
        webkit_path = os.path.join(self.host_app_path, 'WebKitFrameworks')
      else:
        webkit_path = os.path.join(self.test_app_path, 'WebKitFrameworks')
      dyld_path = dyld_path + ':' + webkit_path

    module_data = {
        'TestBundlePath': self.test_app_path,
        'TestHostPath': self.test_app_path,
        'TestHostBundleIdentifier': get_bundle_id(self.test_app_path),
        'TestingEnvironmentVariables': {
            'DYLD_LIBRARY_PATH':
                '%s:__PLATFORMS__/iPhoneSimulator.platform/Developer/Library' %
                dyld_path,
            'DYLD_FRAMEWORK_PATH':
                '%s:__PLATFORMS__/iPhoneSimulator.platform/'
                'Developer/Library/Frameworks' % dyld_path,
        }
    }

    inserted_libs = self.inserted_libs.copy()
    inserted_libs.extend(self._additional_inserted_libs())
    if inserted_libs:
      module_data['TestingEnvironmentVariables'][
          'DYLD_INSERT_LIBRARIES'] = ':'.join(inserted_libs)

    xctestrun_data = {module: module_data}
    gtest_filter = []

    if self.included_tests or self.excluded_tests:
      gtest_filter = get_gtest_filter(self.included_tests, self.excluded_tests)
      # Removed previous gtest-filter if exists.
      self.test_args = [el for el in self.test_args
                        if not el.startswith('--gtest_filter=')]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    if self.repeat_count > 1:
      self.test_args.append('--gtest_repeat=%s' % self.repeat_count)

    if self.env_vars:
      xctestrun_data[module].update({'EnvironmentVariables': self.env_vars})

    self.test_args.append(GENERATE_COMPILED_GTESTS_FILE_TEST_ARG)
    if self.test_args:
      xctestrun_data[module].update({'CommandLineArguments': self.test_args})

    if self.excluded_tests:
      xctestrun_data[module].update({
          'SkipTestIdentifiers': [
              self._replace_multiple_slashes(x) for x in self.excluded_tests
          ]
      })
    if self.included_tests:
      xctestrun_data[module].update({
          'OnlyTestIdentifiers': [
              self._replace_multiple_slashes(x) for x in self.included_tests
          ]
      })
    return xctestrun_data

  def command(self, out_dir, destination, clones):
    """Returns the command that launches tests using xcodebuild.

    Format of command:
    xcodebuild test-without-building -xctestrun file.xctestrun \
      -parallel-testing-enabled YES -parallel-testing-worker-count %d% \
      [-destination "destination"]  -resultBundlePath %output_path%

    Args:
      out_dir: (str) An output directory.
      destination: (str) A destination of running simulator.
      clones: (int) A number of simulator clones to run tests against.

    Returns:
      A list of strings forming the command to launch the test.
    """
    cmd = []
    if is_running_rosetta():
      cmd.extend(['arch', '-arch', 'arm64'])
    cmd.extend([
        'xcodebuild', 'test-without-building', '-xctestrun',
        self.fill_xctest_run(out_dir), '-destination', destination,
        '-resultBundlePath', out_dir
    ])
    if clones > 1:
      cmd.extend([
          '-parallel-testing-enabled', 'YES', '-parallel-testing-worker-count',
          str(clones)
      ])
    return cmd


class EgtestsApp(GTestsApp):
  """Egtests to run.

  Stores data about egtests:
    egtests_app: full path to egtests app.
    project_path: root project folder.
    module_name: egtests module name.
    included_tests: List of tests to run.
    excluded_tests: List of tests not to run.
  """

  def __init__(self, egtests_app: str, all_eg_test_names: List[Tuple[str, str]],
               **kwargs):
    """Initialize Egtests.

    Args:
      egtests_app: (str) full path to egtests app.
      all_eg_test_names: (list) list in the form [(TestCase, testMethod)]
        which contains all the test methods present in the EG test app binary.
      (Following are potential args in **kwargs)
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.
      host_app_path: (str) full path to host app.
      inserted_libs: List of libraries to insert when running the test.
      repeat_count: (int) Number of times to run each test case.
      record_video_option: (enum) If the arg is not none, then video
        recording on tests will be enabled. Currently the enum only supports
        recording on failed tests, but can be extended to support more
        cases in the future if needed.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    super(EgtestsApp, self).__init__(egtests_app, **kwargs)
    self.all_eg_test_names = all_eg_test_names
    self.record_video_option = kwargs.get('record_video_option')

  def get_all_tests(self):
    """Gets all tests to run in this object."""
    all_tests = []
    for test_class, test_method in self.all_eg_test_names:
      test_name = '%s/%s' % (test_class, test_method)

      # |self.initial_included_tests| contains the tests to execute, which
      # may be a subset of all tests b/c of the iOS test sharding logic in
      # shard_util.py. Filter by |self.initial_included_tests| if specified.
      # |self.initial_included_tests| might store test class or full name.
      included = self.initial_included_tests
      if not included or test_name in included or test_class in included:
        all_tests.append(test_name)
    return all_tests

  def _additional_inserted_libs(self):
    """Returns additional libraries to add to inserted_libs."""
    libs = []

    # Do not insert libXCTestBundleInject.dylib if running EG2 or XCUITest
    # tests (which set self.host_app_path), this is no longer needed and
    # broken as of Xcode16 Beta4.
    # However, it is still needed for unit tests which are run as XCTests
    # (and in this case without the GTest framework). See crbug.com/361610467
    # for more details.
    if not self.host_app_path:
      libs.append('__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                  'usr/lib/libXCTestBundleInject.dylib')

    for child in os.listdir(self.test_app_path):
      if child.startswith('libclang_rt.asan'):
        libs.append(os.path.join('@executable_path', child))
    return libs

  def command(self, out_dir, destination, clones):
    """Returns the command that launches tests for EG Tests.

    See details in parent class method docstring. This method appends the
    command line switch if test repeat is required.
    """
    cmd = super(EgtestsApp, self).command(out_dir, destination, clones)
    if self.repeat_count > 1:
      if xcode_util.using_xcode_13_or_higher():
        cmd += ['-test-iterations', str(self.repeat_count)]
      else:
        raise test_runner_errors.XcodeUnsupportedFeatureError(
            'Test repeat is only supported in Xcode 13 or higher!')
    return cmd

  def fill_xctestrun_node(self, include_disabled=False):
    """Fills only required nodes for egtests in xctestrun file.

    Returns:
      A node with filled required fields about egtests.
    """
    xctestrun_data = super(EgtestsApp, self).fill_xctestrun_node()
    module_data = xctestrun_data[self.module_name + '_module']
    module_data['TestBundlePath'] = '__TESTHOST__%s' % xcode_util.xctest_path(
        self.test_app_path)
    module_data['TestingEnvironmentVariables'][
        'XCInjectBundleInto'] = '__TESTHOST__/%s' % self.module_name

    if include_disabled:
      module_data['TestingEnvironmentVariables'][
          'RUN_DISABLED_EARL_GREY_TESTS'] = '1'

    if self.host_app_path:
      # Module data specific to EG2 tests
      module_data['IsUITestBundle'] = True
      module_data['IsXCTRunnerHostedTestBundle'] = True
      module_data['SystemAttachmentLifetime'] = 'keepAlways'
      if self.record_video_option is not None:
        # Currently the enum only supports recording on failed tests,
        # but can be extended to support more cases if needed,
        # such as recording on successful tests.
        module_data['PreferredScreenCaptureFormat'] = 'video'
      else:
        module_data['PreferredScreenCaptureFormat'] = 'screenshots'
      module_data['UITargetAppPath'] = '%s' % self.host_app_path
      module_data['UITargetAppBundleIdentifier'] = get_bundle_id(
          self.host_app_path)
      # Special handling for Xcode10.2
      dependent_products = [
          module_data['UITargetAppPath'],
          module_data['TestBundlePath'],
          module_data['TestHostPath']
      ]
      module_data['DependentProductPaths'] = dependent_products
    # Module data specific to EG1 tests
    else:
      module_data['IsAppHostedTestBundle'] = True

    return xctestrun_data


class DeviceXCTestUnitTestsApp(GTestsApp):
  """XCTest hosted unit tests to run on devices.

  This is for the XCTest framework hosted unit tests running on devices.

  Stores data about tests:
    tests_app: full path to tests app.
    project_path: root project folder.
    module_name: egtests module name.
    included_tests: List of tests to run.
    excluded_tests: List of tests not to run.
  """

  def __init__(self, tests_app, **kwargs):
    """Initialize the class.

    Args:
      tests_app: (str) full path to tests app.
      (Following are potential args in **kwargs)
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching. Test arg to run as XCTest based unit test will be appended.
      env_vars: List of environment variables to pass to the test itself.
      repeat_count: (int) Number of times to run each test case.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    test_args = list(kwargs.get('test_args') or [])
    test_args.append('--enable-run-ios-unittests-with-xctest')
    kwargs['test_args'] = test_args

    super(DeviceXCTestUnitTestsApp, self).__init__(tests_app, **kwargs)

  def fill_xctestrun_node(self):
    """Fills only required nodes for XCTest hosted unit tests in xctestrun file.

    Returns:
      A node with filled required fields about tests.
    """
    xctestrun_data = {
        'TestTargetName': {
            'IsAppHostedTestBundle':
                True,
            'TestBundlePath':
                '__TESTHOST__%s' % xcode_util.xctest_path(self.test_app_path),
            'TestHostBundleIdentifier':
                get_bundle_id(self.test_app_path),
            'TestHostPath':
                '%s' % self.test_app_path,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES':
                    '__TESTHOST__/Frameworks/libXCTestBundleInject.dylib',
                'DYLD_LIBRARY_PATH':
                    '__PLATFORMS__/iPhoneOS.platform/Developer/Library',
                'DYLD_FRAMEWORK_PATH':
                    '__PLATFORMS__/iPhoneOS.platform/Developer/'
                    'Library/Frameworks',
                'XCInjectBundleInto':
                    '__TESTHOST__/%s' % self.module_name
            }
        }
    }

    if self.env_vars:
      xctestrun_data['TestTargetName'].update(
          {'EnvironmentVariables': self.env_vars})

    if self.included_tests or self.excluded_tests:
      gtest_filter = get_gtest_filter(self.included_tests, self.excluded_tests)
      # Removed previous gtest-filter if exists.
      self.test_args = [
          el for el in self.test_args if not el.startswith('--gtest_filter=')
      ]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    if self.repeat_count > 1:
      self.test_args.append('--gtest_repeat=%s' % self.repeat_count)

    self.test_args.append('--gmock_verbose=error')
    self.test_args.append(GENERATE_COMPILED_GTESTS_FILE_TEST_ARG)

    xctestrun_data['TestTargetName'].update(
        {'CommandLineArguments': self.test_args})

    return xctestrun_data


class SimulatorXCTestUnitTestsApp(GTestsApp):
  """XCTest hosted unit tests to run on simulators.

  This is for the XCTest framework hosted unit tests running on simulators.

  Stores data about tests:
    tests_app: full path to tests app.
    project_path: root project folder.
    module_name: egtests module name.
    included_tests: List of tests to run.
    excluded_tests: List of tests not to run.
  """

  def __init__(self, tests_app, **kwargs):
    """Initialize the class.

    Args:
      tests_app: (str) full path to tests app.
      (Following are potential args in **kwargs)
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching. Test arg to run as XCTest based unit test will be appended.
      env_vars: List of environment variables to pass to the test itself.
      repeat_count: (int) Number of times to run each test case.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    test_args = list(kwargs.get('test_args') or [])
    test_args.append('--enable-run-ios-unittests-with-xctest')
    kwargs['test_args'] = test_args
    super(SimulatorXCTestUnitTestsApp, self).__init__(tests_app, **kwargs)

  def fill_xctestrun_node(self):
    """Fills only required nodes for XCTest hosted unit tests in xctestrun file.

    Returns:
      A node with filled required fields about tests.
    """
    xctestrun_data = {
        'TestTargetName': {
            'IsAppHostedTestBundle':
                True,
            'TestBundlePath':
                '__TESTHOST__%s' % xcode_util.xctest_path(self.test_app_path),
            'TestHostBundleIdentifier':
                get_bundle_id(self.test_app_path),
            'TestHostPath':
                '%s' % self.test_app_path,
            'TestingEnvironmentVariables': {
                'DYLD_INSERT_LIBRARIES':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/usr/lib/'
                    'libXCTestBundleInject.dylib',
                'DYLD_LIBRARY_PATH':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/Library',
                'DYLD_FRAMEWORK_PATH':
                    '__PLATFORMS__/iPhoneSimulator.platform/Developer/'
                    'Library/Frameworks',
                'XCInjectBundleInto':
                    '__TESTHOST__/%s' % self.module_name
            }
        }
    }

    if self.env_vars:
      xctestrun_data['TestTargetName'].update(
          {'EnvironmentVariables': self.env_vars})

    if self.included_tests or self.excluded_tests:
      gtest_filter = get_gtest_filter(self.included_tests, self.excluded_tests)
      # Removed previous gtest-filter if exists.
      self.test_args = [
          el for el in self.test_args if not el.startswith('--gtest_filter=')
      ]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    if self.repeat_count > 1:
      self.test_args.append('--gtest_repeat=%s' % self.repeat_count)

    self.test_args.append('--gmock_verbose=error')
    self.test_args.append(GENERATE_COMPILED_GTESTS_FILE_TEST_ARG)

    xctestrun_data['TestTargetName'].update(
        {'CommandLineArguments': self.test_args})

    return xctestrun_data
