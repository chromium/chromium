# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runner for running tests using xcodebuild."""

import collections
import logging
import os
import subprocess
import sys
import time

import file_util
import iossim_util
import test_apps
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner
import xcode_log_parser

# if the current directory is in scripts, then we need to add plugin
# path in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
      os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin'))
from plugin_utils import init_plugins_from_args
from test_plugin_service import TestPluginServicerWrapper, TestPluginServicer

LOGGER = logging.getLogger(__name__)
MAXIMUM_TESTS_PER_SHARD_FOR_RERUN = 20
XTDEVICE_FOLDER = os.path.expanduser('~/Library/Developer/XCTestDevices')


def _tests_decided_at_runtime(app_name):
  """Return if tests in app are selected at runtime by app_name.

  This works for suites defined in chromium infra.
  """
  suite_name_fragments = ['ios_chrome_multitasking_eg', '_flaky_eg']
  return any(fragment in app_name for fragment in suite_name_fragments)


def erase_all_simulators(path=None):
  """Erases all simulator devices.

  Args:
    path: (str) A path with simulators
  """
  command = ['xcrun', 'simctl']
  if path:
    command += ['--set', path]
    LOGGER.info('Erasing all simulators from folder %s.' % path)
  else:
    LOGGER.info('Erasing all simulators.')

  try:
    subprocess.check_call(command + ['erase', 'all'])
  except subprocess.CalledProcessError as e:
    # Logging error instead of throwing so we don't cause failures in case
    # this was indeed failing to clean up.
    message = 'Failed to erase all simulators. Error: %s' % e.output
    LOGGER.error(message)


def shutdown_all_simulators(path=None):
  """Shutdown all simulator devices.

  Fix for DVTCoreSimulatorAdditionsErrorDomain error.

  Args:
    path: (str) A path with simulators
  """
  command = ['xcrun', 'simctl']
  if path:
    command += ['--set', path]
    LOGGER.info('Shutdown all simulators from folder %s.' % path)
  else:
    LOGGER.info('Shutdown all simulators.')

  try:
    subprocess.check_call(command + ['shutdown', 'all'])
  except subprocess.CalledProcessError as e:
    # Logging error instead of throwing so we don't cause failures in case
    # this was indeed failing to clean up.
    message = 'Failed to shutdown all simulators. Error: %s' % e.output
    LOGGER.error(message)


def terminate_process(proc):
  """Terminates the process.

  If an error occurs ignore it, just print out a message.

  Args:
    proc: A subprocess.
  """
  try:
    proc.terminate()
  except OSError as ex:
    LOGGER.error('Error while killing a process: %s' % ex)


class LaunchCommand(object):
  """Stores xcodebuild test launching command."""

  def __init__(self,
               egtests_app,
               udid,
               shards,
               retries,
               readline_timeout,
               out_dir=os.path.basename(os.getcwd()),
               use_clang_coverage=False,
               env=None,
               test_plugin_service=None,
               cert_path=None):
    """Initialize launch command.

    Args:
      egtests_app: (EgtestsApp) An egtests_app to run.
      udid: (str) UDID of a device/simulator.
      shards: (int) A number of shards.
      readline_timeout: (int) Timeout to kill a test process when it doesn't
        have output (in seconds).
      retries: (int) A number of retries.
      out_dir: (str) A folder in which xcodebuild will generate test output.
        By default it is a current directory.
      env: (dict) Environment variables.
      cert_path: (str) A path for cert to install.

    Raises:
      AppNotFoundError: At incorrect egtests_app parameter type.
    """
    if not isinstance(egtests_app, test_apps.EgtestsApp):
      raise test_runner.AppNotFoundError(
          'Parameter `egtests_app` is not EgtestsApp: %s' % egtests_app)
    self.egtests_app = egtests_app
    self.udid = udid
    self.shards = shards
    self.readline_timeout = readline_timeout
    self.retries = retries
    self.out_dir = out_dir
    self.use_clang_coverage = use_clang_coverage
    self.env = env
    self._log_parser = xcode_log_parser.get_parser()
    self.test_plugin_service = test_plugin_service
    self.cert_path = cert_path

  def launch_attempt(self, cmd):
    """Launch a process and do logging simultaneously.

    Args:
      cmd: (list[str]) A command to run.

    Returns:
      output - command output as list of strings.
    """
    proc = subprocess.Popen(
        cmd,
        env=self.env,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    return test_runner.print_process_output(proc, timeout=self.readline_timeout)

  def launch(self):
    """Launches tests using xcodebuild."""
    overall_launch_command_result = ResultCollection()
    shards = self.shards
    running_tests = set(self.egtests_app.get_all_tests())
    # total number of attempts is self.retries+1
    for attempt in range(self.retries + 1):
      # Cleanup any running plugin process before each attempt
      if self.test_plugin_service:
        self.test_plugin_service.reset()
      # Erase all simulators per each attempt
      if iossim_util.is_device_with_udid_simulator(self.udid):
        # kill all running simulators to prevent possible memory leaks
        test_runner.SimulatorTestRunner.kill_simulators()
        shutdown_all_simulators()
        shutdown_all_simulators(XTDEVICE_FOLDER)
        erase_all_simulators()
        erase_all_simulators(XTDEVICE_FOLDER)
        if self.cert_path:
          iossim_util.copy_trusted_certificate(self.cert_path, self.uuid)

        # ideally this should be the last step before running tests, because
        # it boots the simulator.
        iossim_util.disable_simulator_keyboard_tutorial(self.udid)

      outdir_attempt = os.path.join(self.out_dir, 'attempt_%d' % attempt)
      cmd_list = self.egtests_app.command(outdir_attempt, 'id=%s' % self.udid,
                                          shards)
      # TODO(crbug.com/914878): add heartbeat logging to xcodebuild_runner.
      LOGGER.info('Start test attempt #%d for command [%s]' % (
          attempt, ' '.join(cmd_list)))
      output = self.launch_attempt(cmd_list)

      result = self._log_parser.collect_test_results(outdir_attempt, output)

      tests_selected_at_runtime = _tests_decided_at_runtime(
          self.egtests_app.test_app_path)
      # For most suites, only keep crash status from last attempt since retries
      # will cover any missing tests. For these decided at runtime, retain
      # crashes from all attempts and a dummy "crashed" result will be reported
      # to indicate some tests might never ran.
      # TODO(crbug.com/1235871): Switch back to excluded tests and set
      # |overall_crash| to always True.
      overall_launch_command_result.add_result_collection(
          result, overwrite_crash=not tests_selected_at_runtime)
      result.report_to_result_sink()

      tests_to_include = set()
      # |running_tests| are compiled tests in target intersecting with swarming
      # sharding. For some suites, they are more than what's needed to run.
      if not tests_selected_at_runtime:
        tests_to_include = tests_to_include | (
            running_tests - overall_launch_command_result.expected_tests())
      # Add failed tests from last rounds for runtime decided suites and device
      # suites.
      tests_to_include = (
          tests_to_include
          | overall_launch_command_result.never_expected_tests())
      self.egtests_app.included_tests = list(tests_to_include)

      # Nothing to run in retry.
      if not self.egtests_app.included_tests:
        break

      # If tests are not completed(interrupted or did not start) and there are
      # >= 20 remaining tests, run them with the same number of shards.
      # otherwise re-run with shards=1.
      if (not result.crashed
          # If need to re-run less than 20 tests, 1 shard should be enough.
          or (len(running_tests) -
              len(overall_launch_command_result.expected_tests()) <=
              MAXIMUM_TESTS_PER_SHARD_FOR_RERUN)):
        shards = 1

    return overall_launch_command_result


class SimulatorParallelTestRunner(test_runner.SimulatorTestRunner):
  """Class for running simulator tests using xCode."""

  def __init__(self, app_path, host_app_path, iossim_path, version, platform,
               out_dir, **kwargs):
    """Initializes a new instance of SimulatorParallelTestRunner class.

    Args:
      app_path: (str) A path to egtests_app.
      host_app_path: (str) A path to the host app for EG2.
      iossim_path: Path to the compiled iossim binary to use.
                   Not used, but is required by the base class.
      version: (str) iOS version to run simulator on.
      platform: (str) Name of device.
      out_dir: (str) A directory to emit test data into.
      (Following are potential args in **kwargs)
      release: (bool) Whether this test runner is running for a release build.
      repeat_count: (int) Number of times to run each test (passed to test app).
      retries: (int) A number to retry test run, will re-run only failed tests.
      shards: (int) A number of shards. Default is 1.
      test_cases: (list) List of tests to be included in the test run.
                  None or [] to include all tests.
      test_args: List of strings to pass as arguments to the test when
        launching.
      use_clang_coverage: Whether code coverage is enabled in this run.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    kwargs['retries'] = kwargs.get('retries') or 0
    super(SimulatorParallelTestRunner,
          self).__init__(app_path, iossim_path, platform, version, out_dir,
                         **kwargs)
    self.set_up()
    self.host_app_path = None
    if host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
    self.logs = collections.OrderedDict()
    self.release = kwargs.get('release') or False
    self.test_results['path_delimiter'] = '/'

    # TODO(crbug.com/1486897): For simulators, the record_video_option
    # is always None right now, because we are still using our own video
    # plugin. Currently native Xcode15+ video recording is only supported
    # on iOS17+, but we should aim to migrate to the native solution so
    # that we don't need to maintain our own.
    self.record_video_option = kwargs.get('record_video_option')

    # initializing test plugin service
    self.test_plugin_service = None
    enabled_plugins = init_plugins_from_args(
        os.path.join(self.out_dir, self.udid), **kwargs)
    if (len(enabled_plugins) > 0):
      LOGGER.info('Number of enabled plugins are greater than 0, initiating' +
                  'test plugin service... Enabled plugins are %s' %
                  enabled_plugins)
      self.test_plugin_service = TestPluginServicerWrapper(
          TestPluginServicer(enabled_plugins))
    else:
      LOGGER.info('No plugins are enabled, test plugin service will not start.')

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(test_runner.SimulatorTestRunner, self).get_launch_env()
    env['NSUnbufferedIO'] = 'YES'
    return env

  def get_launch_test_app(self):
    """Returns the proper test_app for the run.

    Returns:
      An implementation of EgtestsApp for the runner.
    """
    return test_apps.EgtestsApp(
        self.app_path,
        included_tests=self.test_cases,
        env_vars=self.env_vars,
        test_args=self.test_args,
        release=self.release,
        repeat_count=self.repeat_count,
        host_app_path=self.host_app_path,
        record_video_option=self.record_video_option)

  def launch(self):
    """Launches tests using xcodebuild."""
    if self.test_plugin_service:
      self.test_plugin_service.start_server()
    test_app = self.get_launch_test_app()
    launch_command = LaunchCommand(
        test_app,
        udid=self.udid,
        shards=self.shards,
        readline_timeout=self.readline_timeout,
        retries=self.retries,
        out_dir=os.path.join(self.out_dir, self.udid),
        use_clang_coverage=(hasattr(self, 'use_clang_coverage') and
                            self.use_clang_coverage),
        env=self.get_launch_env(),
        test_plugin_service=self.test_plugin_service)

    try:
      overall_result = launch_command.launch()

      # Deletes simulator used in the tests after tests end.
      if iossim_util.is_device_with_udid_simulator(self.udid):
        iossim_util.delete_simulator_by_udid(self.udid)

      # Adds disabled tests to result.
      if self.output_disabled_tests:
        overall_result.add_and_report_test_names_status(
            launch_command.egtests_app.disabled_tests,
            TestStatus.SKIP,
            expected_status=TestStatus.SKIP,
            test_log='Test disabled.')

      # Adds unexpectedly skipped tests to result if applicable.
      tests_selected_at_runtime = _tests_decided_at_runtime(self.app_path)
      unexpectedly_skipped = []
      # TODO(crbug.com/1048758): For the multitasking or any flaky test suites,
      # |all_tests_to_run| contains more tests than what actually runs.
      if not tests_selected_at_runtime:
        # |all_tests_to_run| takes into consideration that only a subset of
        # tests may have run due to the test sharding logic in run.py.
        all_tests_to_run = set(launch_command.egtests_app.get_all_tests())
        unexpectedly_skipped = list(all_tests_to_run -
                                    overall_result.all_test_names())
        overall_result.add_and_report_test_names_status(
            unexpectedly_skipped,
            TestStatus.SKIP,
            test_log=(
                'The test is compiled in test target but was unexpectedly '
                'not run or not finished.'))

      # Add a final crash status to result collection. It will be reported as
      # part of step log in LUCI build.
      if unexpectedly_skipped or overall_result.crashed:
        overall_result.set_crashed_with_prefix(
            crash_message_prefix_line=(
                'Test application crash happened and may '
                'result in missing tests:'))

      self.test_results = overall_result.standard_json_output(
          path_delimiter='/')
      self.logs.update(overall_result.test_runner_logs())

      # Return False when:
      # - There are unexpected tests (all results of the tests are unexpected),
      # or
      # - The overall status is crashed and tests are selected at runtime. (i.e.
      # runner is unable to know if all scheduled tests appear in result.)
      return (not overall_result.never_expected_tests() and
              not (tests_selected_at_runtime and overall_result.crashed))
    finally:
      self.tear_down()

  def tear_down(self):
    if self.test_plugin_service:
      LOGGER.info('Shutting down test plugin service')
      self.test_plugin_service.tear_down()


class DeviceXcodeTestRunner(SimulatorParallelTestRunner,
                            test_runner.DeviceTestRunner):
  """Class for running tests on real device using xCode."""

  def __init__(self, app_path, host_app_path, out_dir, **kwargs):
    """Initializes a new instance of DeviceXcodeTestRunner class.

    Args:
      app_path: (str) A path to egtests_app.
      host_app_path: (str) A path to the host app for EG2.
      out_dir: (str) A directory to emit test data into.
      (Following are potential args in **kwargs)
      repeat_count: (int) Number of times to run each test (passed to test app).
      retries: (int) A number to retry test run, will re-run only failed tests.
      test_cases: (list) List of tests to be included in the test run.
                  None or [] to include all tests.
      test_args: List of strings to pass as arguments to the test when
        launching.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist.
      DeviceDetectionError: If no device found.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    test_runner.DeviceTestRunner.__init__(self, app_path, out_dir, **kwargs)
    self.shards = 1  # For tests on real devices shards=1
    self.version = None
    self.platform = None
    self.host_app_path = None
    if host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
    self.homedir = ''
    self.release = kwargs.get('release') or False
    self.set_up()
    self.start_time = time.strftime('%Y-%m-%d-%H%M%S', time.localtime())
    self.test_results['path_delimiter'] = '/'
    self.record_video_option = kwargs.get('record_video_option')
    self.test_plugin_service = None

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.uninstall_apps()
    self.wipe_derived_data()
    self.restart_usbmuxd()

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    test_runner.DeviceTestRunner.tear_down(self)
