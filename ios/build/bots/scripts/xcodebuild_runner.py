# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runner for running tests using xcodebuild."""

import collections
import distutils.version
import logging
from multiprocessing import pool
import os
import subprocess
import sys
import time

import file_util
import iossim_util
import standard_json_util as sju
import test_apps
import test_runner
import xcode_log_parser

LOGGER = logging.getLogger(__name__)
MAXIMUM_TESTS_PER_SHARD_FOR_RERUN = 20
XTDEVICE_FOLDER = os.path.expanduser('~/Library/Developer/XCTestDevices')


class LaunchCommandCreationError(test_runner.TestRunnerError):
  """One of launch command parameters was not set properly."""

  def __init__(self, message):
    super(LaunchCommandCreationError, self).__init__(message)


class LaunchCommandPoolCreationError(test_runner.TestRunnerError):
  """Failed to create a pool of launch commands."""

  def __init__(self, message):
    super(LaunchCommandPoolCreationError, self).__init__(message)


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
               out_dir=os.path.basename(os.getcwd()),
               use_clang_coverage=False,
               env=None):
    """Initialize launch command.

    Args:
      egtests_app: (EgtestsApp) An egtests_app to run.
      udid: (str) UDID of a device/simulator.
      shards: (int) A number of shards.
      retries: (int) A number of retries.
      out_dir: (str) A folder in which xcodebuild will generate test output.
        By default it is a current directory.
      env: (dict) Environment variables.

    Raises:
      LaunchCommandCreationError: if one of parameters was not set properly.
    """
    if not isinstance(egtests_app, test_apps.EgtestsApp):
      raise test_runner.AppNotFoundError(
          'Parameter `egtests_app` is not EgtestsApp: %s' % egtests_app)
    self.egtests_app = egtests_app
    self.udid = udid
    self.shards = shards
    self.retries = retries
    self.out_dir = out_dir
    self.logs = collections.OrderedDict()
    self.test_results = collections.OrderedDict()
    self.use_clang_coverage = use_clang_coverage
    self.env = env
    self._log_parser = xcode_log_parser.get_parser()

  def summary_log(self):
    """Calculates test summary - how many passed, failed and error tests.

    Returns:
      Dictionary with number of passed and failed tests.
      Failed tests will be calculated from the last test attempt.
      Passed tests calculated for each test attempt.
    """
    test_statuses = ['passed', 'failed']
    for status in test_statuses:
      self.logs[status] = 0

    for index, test_attempt_results in enumerate(self.test_results['attempts']):
      for test_status in test_statuses:
        if test_status not in test_attempt_results:
          continue
        if (test_status == 'passed'
            # Number of failed tests is taken only from last run.
            or (test_status == 'failed'
                and index == len(self.test_results['attempts']) - 1)):
          self.logs[test_status] += len(test_attempt_results[test_status])

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
    return test_runner.print_process_output(proc)

  def launch(self):
    """Launches tests using xcodebuild."""
    self.test_results['attempts'] = []
    cancelled_statuses = {'TESTS_DID_NOT_START', 'BUILD_INTERRUPTED'}
    shards = self.shards
    running_tests = set(self.egtests_app.get_all_tests())
    passed_tests = set()
    # total number of attempts is self.retries+1
    for attempt in range(self.retries + 1):
      # Erase all simulators per each attempt
      if iossim_util.is_device_with_udid_simulator(self.udid):
        # kill all running simulators to prevent possible memory leaks
        test_runner.SimulatorTestRunner.kill_simulators()
        shutdown_all_simulators()
        shutdown_all_simulators(XTDEVICE_FOLDER)
        erase_all_simulators()
        erase_all_simulators(XTDEVICE_FOLDER)
      outdir_attempt = os.path.join(self.out_dir, 'attempt_%d' % attempt)
      cmd_list = self.egtests_app.command(outdir_attempt, 'id=%s' % self.udid,
                                          shards)
      # TODO(crbug.com/914878): add heartbeat logging to xcodebuild_runner.
      LOGGER.info('Start test attempt #%d for command [%s]' % (
          attempt, ' '.join(cmd_list)))
      output = self.launch_attempt(cmd_list)

      if hasattr(self, 'use_clang_coverage') and self.use_clang_coverage:
        # out_dir of LaunchCommand object is the TestRunner out_dir joined with
        # UDID. Use os.path.dirname to retrieve the TestRunner out_dir.
        file_util.move_raw_coverage_data(self.udid,
                                         os.path.dirname(self.out_dir))
      self.test_results['attempts'].append(
          self._log_parser.collect_test_results(outdir_attempt, output))

      # Do not exit here when no failed test from parsed log and parallel
      # testing is enabled (shards > 1), because when one of the shards fails
      # before tests start , the tests not run don't appear in log at all.
      if (self.retries == attempt or
          (shards == 1 and not self.test_results['attempts'][-1]['failed'])):
        break

      # Exclude passed tests in next test attempt.
      passed_tests = passed_tests | set(
          self.test_results['attempts'][-1]['passed'])
      tests_to_include = set()
      # |running_tests| are compiled tests in target intersecting with swarming
      # sharding. For some suites, they are more than what's needed to run.
      if not _tests_decided_at_runtime(self.egtests_app.test_app_path):
        tests_to_include = tests_to_include | (running_tests - passed_tests)
      # Add failed tests from last round for runtime decided suites and device
      # suites.
      tests_to_include = tests_to_include | (
          set(self.test_results['attempts'][-1]['failed'].keys()) -
          cancelled_statuses)
      self.egtests_app.included_tests = list(tests_to_include)

      # crbug.com/987664 - for the case when
      # all tests passed but build was interrupted,
      # passed tests are equal to tests to run.
      if passed_tests == running_tests or not self.egtests_app.included_tests:
        # Keep cancelled status for these, since some tests might be skipped
        # don't appear in test results.
        if not _tests_decided_at_runtime(self.egtests_app.test_app_path):
          for status in cancelled_statuses:
            failure = self.test_results['attempts'][-1]['failed'].pop(
                status, None)
            if failure:
              LOGGER.info('Failure for passed tests %s: %s' % (status, failure))
        break

      # If tests are not completed(interrupted or did not start)
      # re-run them with the same number of shards,
      # otherwise re-run with shards=1 and exclude passed tests.
      cancelled_attempt = cancelled_statuses.intersection(
          self.test_results['attempts'][-1]['failed'].keys())

      # Item in cancelled_statuses is used to config for next attempt. The usage
      # should be confined in this method. Real tests affected by these statuses
      # will be marked timeout in results.
      for status in cancelled_statuses:
        # Keep cancelled status for these, since some tests might be skipped
        # don't appear in test results.
        if not _tests_decided_at_runtime(self.egtests_app.test_app_path):
          self.test_results['attempts'][-1]['failed'].pop(status, None)

      if (not cancelled_attempt
          # If need to re-run less than 20 tests, 1 shard should be enough.
          or (len(self.egtests_app.included_tests) <=
              MAXIMUM_TESTS_PER_SHARD_FOR_RERUN)):

        shards = 1

    self.summary_log()

    return {
        'test_results': self.test_results,
        'logs': self.logs
    }


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
    kwargs['retries'] = kwargs.get('retries') or 1
    super(SimulatorParallelTestRunner,
          self).__init__(app_path, iossim_path, platform, version, out_dir,
                         **kwargs)
    self.set_up()
    self.host_app_path = None
    if host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
    self._init_sharding_data()
    self.logs = collections.OrderedDict()
    self.release = kwargs.get('release') or False
    self.test_results['path_delimiter'] = '/'
    # Do not enable parallel testing when code coverage is enabled, because raw
    # coverage data won't be produced with parallel testing.
    if hasattr(self, 'use_clang_coverage') and self.use_clang_coverage:
      self.shards = 1

  def _init_sharding_data(self):
    """Initialize sharding data.

    For common case info about sharding tests will be a list of dictionaries:
    [
        {
            'app':paths to egtests_app,
            'udid': 'UDID of Simulator'
            'shards': N
        }
    ]
    """
    self.sharding_data = [{
        'app': self.app_path,
        'host': self.host_app_path,
        'udid': self.udid,
        'shards': self.shards,
        'test_cases': self.test_cases
    }]

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(test_runner.SimulatorTestRunner, self).get_launch_env()
    env['NSUnbufferedIO'] = 'YES'
    return env

  def get_launch_test_app(self, params):
    """Returns the proper test_app for the run, requiring sharding data.

    Args:
      params: A collection of sharding_data params.

    Returns:
      An implementation of EgtestsApp included the sharding_data params
    """
    return test_apps.EgtestsApp(
        params['app'],
        included_tests=params['test_cases'],
        env_vars=self.env_vars,
        test_args=self.test_args,
        release=self.release,
        host_app_path=params['host'])

  def launch(self):
    """Launches tests using xcodebuild."""
    launch_commands = []
    for params in self.sharding_data:
      test_app = self.get_launch_test_app(params)
      launch_commands.append(
          LaunchCommand(
              test_app,
              udid=params['udid'],
              shards=params['shards'],
              retries=self.retries,
              out_dir=os.path.join(self.out_dir, params['udid']),
              use_clang_coverage=(hasattr(self, 'use_clang_coverage') and
                                  self.use_clang_coverage),
              env=self.get_launch_env()))

    thread_pool = pool.ThreadPool(len(launch_commands))
    attempts_results = []
    for result in thread_pool.imap_unordered(LaunchCommand.launch,
                                             launch_commands):
      attempts_results.append(result['test_results']['attempts'])

    # Deletes simulator used in the tests after tests end.
    if iossim_util.is_device_with_udid_simulator(self.udid):
      iossim_util.delete_simulator_by_udid(self.udid)

    # Gets passed tests
    self.logs['passed tests'] = []
    for shard_attempts in attempts_results:
      for attempt in shard_attempts:
        self.logs['passed tests'].extend(attempt['passed'])

    # If the last attempt does not have failures, mark failed as empty
    self.logs['failed tests'] = []
    for shard_attempts in attempts_results:
      if shard_attempts[-1]['failed']:
        self.logs['failed tests'].extend(shard_attempts[-1]['failed'].keys())

    # Gets disabled tests from test app object if any.
    self.logs['disabled tests'] = []
    for launch_command in launch_commands:
      self.logs['disabled tests'].extend(
          launch_command.egtests_app.disabled_tests)

    # Gets all failures/flakes and lists them in bot summary
    all_failures = set()
    for shard_attempts in attempts_results:
      for attempt, attempt_results in enumerate(shard_attempts):
        for failure in attempt_results['failed']:
          if failure not in self.logs:
            self.logs[failure] = []
          self.logs[failure].append('%s: attempt # %d' % (failure, attempt))
          self.logs[failure].extend(attempt_results['failed'][failure])
          all_failures.add(failure)

    # Gets only flaky(not failed) tests.
    self.logs['flaked tests'] = list(
        all_failures - set(self.logs['failed tests']))

    # Gets not-started/interrupted tests.
    # all_tests_to_run takes into consideration that only a subset of tests may
    # have run due to the test sharding logic in run.py.
    all_tests_to_run = set([
        test_name for launch_command in launch_commands
        for test_name in launch_command.egtests_app.get_all_tests()
    ])

    aborted_tests = []
    # TODO(crbug.com/1048758): For the multitasking or any flaky test suites,
    # |all_tests_to_run| contains more tests than what actually runs.
    if not _tests_decided_at_runtime(self.app_path):
      aborted_tests = list(all_tests_to_run - set(self.logs['failed tests']) -
                           set(self.logs['passed tests']))
    aborted_tests.sort()
    self.logs['aborted tests'] = aborted_tests

    self.test_results['interrupted'] = bool(aborted_tests)
    self.test_results['num_failures_by_type'] = {
        'FAIL': len(self.logs['failed tests'] + self.logs['aborted tests']),
        'PASS': len(self.logs['passed tests']),
    }

    output = sju.StdJson()
    for shard_attempts in attempts_results:
      for attempt, attempt_results in enumerate(shard_attempts):

        for test in attempt_results['failed'].keys():
          output.mark_failed(test, test_log='\n'.join(self.logs.get(test, [])))

        # 'aborted tests' in logs is an array of strings, each string defined
        # as "{TestCase}/{testMethod}"
        for test in self.logs['aborted tests']:
          output.mark_timeout(test)

        for test in attempt_results['passed']:
          output.mark_passed(test)

    output.mark_all_disabled(self.logs['disabled tests'])
    output.finalize()

    self.test_results['tests'] = output.tests

    # Test is failed if there are failures for the last run.
    # or if there are aborted tests.
    return not self.logs['failed tests'] and not self.logs['aborted tests']


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
    self._init_sharding_data()
    self.start_time = time.strftime('%Y-%m-%d-%H%M%S', time.localtime())
    self.test_results['path_delimiter'] = '/'

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.uninstall_apps()
    self.wipe_derived_data()

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    test_runner.DeviceTestRunner.tear_down(self)

  def launch(self):
    try:
      return super(DeviceXcodeTestRunner, self).launch()
    finally:
      self.tear_down()
