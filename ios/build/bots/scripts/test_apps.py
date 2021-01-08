# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test apps for running tests using xcodebuild."""

import os
import plistlib
import subprocess
import time

import shard_util
import test_runner


OUTPUT_DISABLED_TESTS_TEST_ARG = '--write-compiled-tests-json-to-writable-path'


#TODO(crbug.com/1046911): Remove usage of KIF filters.
def get_kif_test_filter(tests, invert=False):
  """Returns the KIF test filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to GKIF_SCENARIO_FILTER.
  """
  # A pipe-separated list of test cases with the "KIF." prefix omitted.
  # e.g. NAME:a|b|c matches KIF.a, KIF.b, KIF.c.
  # e.g. -NAME:a|b|c matches everything except KIF.a, KIF.b, KIF.c.
  test_filter = '|'.join(test.split('KIF.', 1)[-1] for test in tests)
  if invert:
    return '-NAME:%s' % test_filter
  return 'NAME:%s' % test_filter


def get_gtest_filter(tests, invert=False):
  """Returns the GTest filter to filter the given test cases.

  Args:
    tests: List of test cases to filter.
    invert: Whether to invert the filter or not. Inverted, the filter will match
      everything except the given test cases.

  Returns:
    A string which can be supplied to --gtest_filter.
  """
  # A colon-separated list of tests cases.
  # e.g. a:b:c matches a, b, c.
  # e.g. -a:b:c matches everything except a, b, c.
  test_filter = ':'.join(test for test in tests)
  if invert:
    return '-%s' % test_filter
  return test_filter


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
  ]).rstrip()


class GTestsApp(object):
  """Gtests app to run.

  Stores data about egtests:
    test_app: full path to an app.
  """

  def __init__(self,
               test_app,
               included_tests=None,
               excluded_tests=None,
               test_args=None,
               env_vars=None,
               release=False,
               host_app_path=None):
    """Initialize Egtests.

    Args:
      test_app: (str) full path to egtests app.
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

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    if not os.path.exists(test_app):
      raise test_runner.AppNotFoundError(test_app)
    self.test_app_path = test_app
    self.project_path = os.path.dirname(self.test_app_path)
    self.test_args = test_args or []
    self.env_vars = {}
    for env_var in env_vars or []:
      env_var = env_var.split('=', 1)
      self.env_vars[env_var[0]] = None if len(env_var) == 1 else env_var[1]
    self.included_tests = included_tests or []
    self.excluded_tests = excluded_tests or []
    self.disabled_tests = []
    self.module_name = os.path.splitext(os.path.basename(test_app))[0]
    self.release = release
    self.host_app_path = host_app_path

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
    if not os.path.exists(xctestrun):
      with open(xctestrun, 'w'):
        pass
    # Creates a dict with data about egtests to run - fill all required fields:
    # egtests_module, egtest_app_path, egtests_xctest_path and
    # filtered tests if filter is specified.
    # Write data in temp xctest run file.
    plistlib.writePlist(self.fill_xctestrun_node(), xctestrun)
    return xctestrun

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

    xctestrun_data = {module: module_data}
    kif_filter = []
    gtest_filter = []

    if self.included_tests:
      kif_filter = get_kif_test_filter(self.included_tests, invert=False)
      gtest_filter = get_gtest_filter(self.included_tests, invert=False)
    elif self.excluded_tests:
      kif_filter = get_kif_test_filter(self.excluded_tests, invert=True)
      gtest_filter = get_gtest_filter(self.excluded_tests, invert=True)

    if kif_filter:
      self.env_vars['GKIF_SCENARIO_FILTER'] = gtest_filter
    if gtest_filter:
      # Removed previous gtest-filter if exists.
      self.test_args = [el for el in self.test_args
                        if not el.startswith('--gtest_filter=')]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    if self.env_vars:
      xctestrun_data[module].update({'EnvironmentVariables': self.env_vars})
    if self.test_args:
      xctestrun_data[module].update({'CommandLineArguments': self.test_args})

    if self.excluded_tests:
      xctestrun_data[module].update({
          'SkipTestIdentifiers': self.excluded_tests
      })
    if self.included_tests:
      xctestrun_data[module].update({
          'OnlyTestIdentifiers': self.included_tests
      })
    return xctestrun_data

  def command(self, out_dir, destination, shards):
    """Returns the command that launches tests using xcodebuild.

    Format of command:
    xcodebuild test-without-building -xctestrun file.xctestrun \
      -parallel-testing-enabled YES -parallel-testing-worker-count %d% \
      [-destination "destination"]  -resultBundlePath %output_path%

    Args:
      out_dir: (str) An output directory.
      destination: (str) A destination of running simulator.
      shards: (int) A number of shards.

    Returns:
      A list of strings forming the command to launch the test.
    """
    cmd = [
        'xcodebuild', 'test-without-building',
        '-xctestrun', self.fill_xctest_run(out_dir),
        '-destination', destination,
        '-resultBundlePath', out_dir
    ]
    if shards > 1:
      cmd += ['-parallel-testing-enabled', 'YES',
              '-parallel-testing-worker-count', str(shards)]
    return cmd

  def get_all_tests(self):
    """Gets all tests to run in this object."""
    # Method names that starts with test* and also are in *TestCase classes
    # but they are not test-methods.
    # TODO(crbug.com/982435): Rename not test methods with test-suffix.
    none_tests = ['ChromeTestCase/testServer', 'FindInPageTestCase/testURL']
    # TODO(crbug.com/1123681): Move all_tests to class var. Set all_tests,
    # disabled_tests values in initialization to avoid multiple calls to otool.
    all_tests = []
    # Only store the tests when there is the test arg.
    store_disabled_tests = OUTPUT_DISABLED_TESTS_TEST_ARG in self.test_args
    self.disabled_tests = []
    for test_class, test_method in shard_util.fetch_test_names(
        self.test_app_path,
        self.host_app_path,
        self.release,
        enabled_tests_only=False):
      test_name = '%s/%s' % (test_class, test_method)
      if (test_name not in none_tests and
          # inlcuded_tests contains the tests to execute, which may be a subset
          # of all tests b/c of the iOS test sharding logic in run.py. Filter by
          # self.included_tests if specified
          (test_class in self.included_tests if self.included_tests else True)):
        if test_method.startswith('test'):
          all_tests.append(test_name)
        elif store_disabled_tests:
          self.disabled_tests.append(test_name)
    return all_tests


class EgtestsApp(GTestsApp):
  """Egtests to run.

  Stores data about egtests:
    egtests_app: full path to egtests app.
    project_path: root project folder.
    module_name: egtests module name.
    included_tests: List of tests to run.
    excluded_tests: List of tests not to run.
  """

  def __init__(self,
               egtests_app,
               included_tests=None,
               excluded_tests=None,
               test_args=None,
               env_vars=None,
               release=False,
               host_app_path=None):
    """Initialize Egtests.

    Args:
      egtests_app: (str) full path to egtests app.
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

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    super(EgtestsApp,
          self).__init__(egtests_app, included_tests, excluded_tests, test_args,
                         env_vars, release, host_app_path)

  def _xctest_path(self):
    """Gets xctest-file from egtests/PlugIns folder.

    Returns:
      A path for xctest in the format of /PlugIns/file.xctest

    Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
    """
    plugins_dir = os.path.join(self.test_app_path, 'PlugIns')
    if not os.path.exists(plugins_dir):
      raise test_runner.PlugInsNotFoundError(plugins_dir)
    plugin_xctest = None
    if os.path.exists(plugins_dir):
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          plugin_xctest = os.path.join(plugins_dir, plugin)
    if not plugin_xctest:
      raise test_runner.XCTestPlugInNotFoundError(plugin_xctest)
    return plugin_xctest.replace(self.test_app_path, '')

  def fill_xctestrun_node(self):
    """Fills only required nodes for egtests in xctestrun file.

    Returns:
      A node with filled required fields about egtests.
    """
    xctestrun_data = super(EgtestsApp, self).fill_xctestrun_node()
    module_data = xctestrun_data[self.module_name + '_module']

    module_data['TestingEnvironmentVariables']['DYLD_INSERT_LIBRARIES'] = (
        '__PLATFORMS__/iPhoneSimulator.platform/Developer/'
        'usr/lib/libXCTestBundleInject.dylib')
    module_data['TestBundlePath'] = '__TESTHOST__/%s' % self._xctest_path()
    module_data['TestingEnvironmentVariables'][
        'XCInjectBundleInto'] = '__TESTHOST__/%s' % self.module_name

    if self.host_app_path:
      # Module data specific to EG2 tests
      module_data['IsUITestBundle'] = True
      module_data['IsXCTRunnerHostedTestBundle'] = True
      module_data['UITargetAppPath'] = '%s' % self.host_app_path
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

  def __init__(self,
               tests_app,
               included_tests=None,
               excluded_tests=None,
               test_args=None,
               env_vars=None,
               release=False):
    """Initialize the class.

    Args:
      tests_app: (str) full path to tests app.
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching. Test arg to run as XCTest based unit test will be appended.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    test_args = list(test_args or [])
    test_args.append('--enable-run-ios-unittests-with-xctest')
    super(DeviceXCTestUnitTestsApp,
          self).__init__(tests_app, included_tests, excluded_tests, test_args,
                         env_vars, release, None)

  # TODO(crbug.com/1077277): Refactor class structure and remove duplicate code.
  def _xctest_path(self):
    """Gets xctest-file from egtests/PlugIns folder.

    Returns:
      A path for xctest in the format of /PlugIns/file.xctest

    Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
    """
    plugins_dir = os.path.join(self.test_app_path, 'PlugIns')
    if not os.path.exists(plugins_dir):
      raise test_runner.PlugInsNotFoundError(plugins_dir)
    plugin_xctest = None
    if os.path.exists(plugins_dir):
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          plugin_xctest = os.path.join(plugins_dir, plugin)
    if not plugin_xctest:
      raise test_runner.XCTestPlugInNotFoundError(plugin_xctest)
    return plugin_xctest.replace(self.test_app_path, '')

  def fill_xctestrun_node(self):
    """Fills only required nodes for XCTest hosted unit tests in xctestrun file.

    Returns:
      A node with filled required fields about tests.
    """
    xctestrun_data = {
        'TestTargetName': {
            'IsAppHostedTestBundle': True,
            'TestBundlePath': '__TESTHOST__/%s' % self._xctest_path(),
            'TestHostBundleIdentifier': get_bundle_id(self.test_app_path),
            'TestHostPath': '%s' % self.test_app_path,
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
      self.xctestrun_data['TestTargetName'].update(
          {'EnvironmentVariables': self.env_vars})

    gtest_filter = []
    if self.included_tests:
      gtest_filter = get_gtest_filter(self.included_tests, invert=False)
    elif self.excluded_tests:
      gtest_filter = get_gtest_filter(self.excluded_tests, invert=True)
    if gtest_filter:
      # Removed previous gtest-filter if exists.
      self.test_args = [
          el for el in self.test_args if not el.startswith('--gtest_filter=')
      ]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    self.test_args.append('--gmock_verbose=error')

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

  def __init__(self,
               tests_app,
               included_tests=None,
               excluded_tests=None,
               test_args=None,
               env_vars=None,
               release=False):
    """Initialize the class.

    Args:
      tests_app: (str) full path to tests app.
      included_tests: (list) Specific tests to run
         E.g.
          [ 'TestCaseClass1/testMethod1', 'TestCaseClass2/testMethod2']
      excluded_tests: (list) Specific tests not to run
         E.g.
          [ 'TestCaseClass1', 'TestCaseClass2/testMethod2']
      test_args: List of strings to pass as arguments to the test when
        launching. Test arg to run as XCTest based unit test will be appended.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist
    """
    test_args = list(test_args or [])
    test_args.append('--enable-run-ios-unittests-with-xctest')
    super(SimulatorXCTestUnitTestsApp,
          self).__init__(tests_app, included_tests, excluded_tests, test_args,
                         env_vars, release, None)

  # TODO(crbug.com/1077277): Refactor class structure and remove duplicate code.
  def _xctest_path(self):
    """Gets xctest-file from egtests/PlugIns folder.

    Returns:
      A path for xctest in the format of /PlugIns/file.xctest

    Raises:
      PlugInsNotFoundError: If no PlugIns folder found in egtests.app.
      XCTestPlugInNotFoundError: If no xctest-file found in PlugIns.
    """
    plugins_dir = os.path.join(self.test_app_path, 'PlugIns')
    if not os.path.exists(plugins_dir):
      raise test_runner.PlugInsNotFoundError(plugins_dir)
    plugin_xctest = None
    if os.path.exists(plugins_dir):
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          plugin_xctest = os.path.join(plugins_dir, plugin)
    if not plugin_xctest:
      raise test_runner.XCTestPlugInNotFoundError(plugin_xctest)
    return plugin_xctest.replace(self.test_app_path, '')

  def fill_xctestrun_node(self):
    """Fills only required nodes for XCTest hosted unit tests in xctestrun file.

    Returns:
      A node with filled required fields about tests.
    """
    xctestrun_data = {
        'TestTargetName': {
            'IsAppHostedTestBundle': True,
            'TestBundlePath': '__TESTHOST__/%s' % self._xctest_path(),
            'TestHostBundleIdentifier': get_bundle_id(self.test_app_path),
            'TestHostPath': '%s' % self.test_app_path,
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
      self.xctestrun_data['TestTargetName'].update(
          {'EnvironmentVariables': self.env_vars})

    gtest_filter = []
    if self.included_tests:
      gtest_filter = get_gtest_filter(self.included_tests, invert=False)
    elif self.excluded_tests:
      gtest_filter = get_gtest_filter(self.excluded_tests, invert=True)
    if gtest_filter:
      # Removed previous gtest-filter if exists.
      self.test_args = [
          el for el in self.test_args if not el.startswith('--gtest_filter=')
      ]
      self.test_args.append('--gtest_filter=%s' % gtest_filter)

    self.test_args.append('--gmock_verbose=error')

    xctestrun_data['TestTargetName'].update(
        {'CommandLineArguments': self.test_args})

    return xctestrun_data
