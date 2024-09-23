# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runners for iOS."""

import errno
import signal
import sys

import collections
import json
import logging
import os
import psutil
import shutil
import subprocess
import threading
import time
from typing import List, Optional

import constants
import file_util
import gtest_utils
import mac_util
import iossim_util
import shard_util
import test_apps
from test_result_util import ResultCollection, TestResult, TestStatus
import test_runner_errors
from xcode_log_parser import XcodeLogParser
import xcode_util
import xctest_utils

LOGGER = logging.getLogger(__name__)
DERIVED_DATA = os.path.expanduser('~/Library/Developer/Xcode/DerivedData')
DEFAULT_TEST_REPO = 'https://chromium.googlesource.com/chromium/src'
HOST_IS_DOWN_ERROR = 'Domain=NSPOSIXErrorDomain Code=64 "Host is down"'
MIG_SERVER_DIED_ERROR = '(ipc/mig) server died'


# TODO(crbug.com/40129082): Move commonly used error classes to
# test_runner_errors module.
class TestRunnerError(test_runner_errors.Error):
  """Base class for TestRunner-related errors."""
  pass


class DeviceError(TestRunnerError):
  """Base class for physical device related errors."""
  pass


class AppLaunchError(TestRunnerError):
  """The app failed to launch."""
  pass


class AppNotFoundError(TestRunnerError):
  """The requested app was not found."""
  def __init__(self, app_path):
    super(AppNotFoundError, self).__init__(
      'App does not exist: %s' % app_path)


class SystemAlertPresentError(DeviceError):
  """System alert is shown on the device."""
  def __init__(self):
    super(SystemAlertPresentError, self).__init__(
      'System alert is shown on the device.')


class DeviceDetectionError(DeviceError):
  """Unexpected number of devices detected."""
  def __init__(self, udids):
    super(DeviceDetectionError, self).__init__(
      'Expected one device, found %s:\n%s' % (len(udids), '\n'.join(udids)))


class DeviceRestartError(DeviceError):
  """Error restarting a device."""
  def __init__(self):
    super(DeviceRestartError, self).__init__('Error restarting a device')


class PlugInsNotFoundError(TestRunnerError):
  """The PlugIns directory was not found."""
  def __init__(self, plugins_dir):
    super(PlugInsNotFoundError, self).__init__(
      'PlugIns directory does not exist: %s' % plugins_dir)


class SimulatorNotFoundError(TestRunnerError):
  """The given simulator binary was not found."""
  def __init__(self, iossim_path):
    super(SimulatorNotFoundError, self).__init__(
        'Simulator does not exist: %s' % iossim_path)


class TestDataExtractionError(DeviceError):
  """Error extracting test data or crash reports from a device."""
  def __init__(self):
    super(TestDataExtractionError, self).__init__('Failed to extract test data')


class XcodeVersionNotFoundError(TestRunnerError):
  """The requested version of Xcode was not found."""
  def __init__(self, xcode_version):
    super(XcodeVersionNotFoundError, self).__init__(
        'Xcode version not found: %s' % xcode_version)


class XCTestConfigError(TestRunnerError):
  """Error related with XCTest config."""

  def __init__(self, message):
    super(XCTestConfigError,
          self).__init__('Incorrect config related with XCTest: %s' % message)


class XCTestPlugInNotFoundError(TestRunnerError):
  """The .xctest PlugIn was not found."""
  def __init__(self, xctest_path):
    super(XCTestPlugInNotFoundError, self).__init__(
        'XCTest not found: %s' % xctest_path)


class ParallelSimDisabledError(TestRunnerError):
  """Temporary error indicating that running tests in parallel on
   simulator clones is not yet implemented."""

  def __init__(self):
    super(ParallelSimDisabledError, self).__init__(
        'Running in parallel on simulator clones has not been implemented!')


def get_device_ios_version(udid):
  """Gets device iOS version.

  Args:
    udid: (str) iOS device UDID.

  Returns:
    Device UDID.
  """
  return subprocess.check_output(
      ['ideviceinfo', '--udid', udid, '-k',
       'ProductVersion']).decode('utf-8').strip()


def defaults_write(d, key, value):
  """Run 'defaults write d key value' command.

  Args:
    d: (str) A dictionary.
    key: (str) A key.
    value: (str) A value.
  """
  LOGGER.info('Run \'defaults write %s %s %s\'' % (d, key, value))
  subprocess.call(['defaults', 'write', d, key, value])


def defaults_delete(d, key):
  """Run 'defaults delete d key' command.

  Args:
    d: (str) A dictionary.
    key: (str) Key to delete.
  """
  LOGGER.info('Run \'defaults delete %s %s\'' % (d, key))
  subprocess.call(['defaults', 'delete', d, key])


def terminate_process(proc, proc_name):
  """Terminates the process.

  If an error occurs ignore it, just print out a message.

  Args:
    proc: A subprocess to terminate.
    proc_name: A name of process.
  """
  try:
    LOGGER.info('Killing hung process %s' % proc.pid)
    proc.terminate()
    attempts_to_kill = 3
    ps = psutil.Process(proc.pid)
    for _ in range(attempts_to_kill):
      # Check whether proc.pid process is still alive.
      if ps.is_running():
        LOGGER.info(
            'Process %s is still alive! %s process might block it.',
            psutil.Process(proc.pid).name(), proc_name)
        running_processes = [
            p for p in psutil.process_iter()
            # Use as_dict() to avoid API changes across versions of psutil.
            if proc_name == p.as_dict(attrs=['name'])['name']]
        if not running_processes:
          LOGGER.debug('There are no running %s processes.', proc_name)
          break
        LOGGER.debug('List of running %s processes: %s'
                     % (proc_name, running_processes))
        # Killing running processes with proc_name
        for p in running_processes:
          p.send_signal(signal.SIGKILL)
        psutil.wait_procs(running_processes)
      else:
        LOGGER.info('Process was killed!')
        break
  except OSError as ex:
    LOGGER.info('Error while killing a process: %s' % ex)


# TODO(crbug.com/40115765): Moved print_process_output to utils class.
def print_process_output(proc,
                         proc_name=None,
                         parser=None,
                         timeout=constants.READLINE_TIMEOUT):
  """Logs process messages in console and waits until process is done.

  Method waits until no output message and if no message for timeout seconds,
  process will be terminated.

  Args:
    proc: A running process.
    proc_name: (str) A process name that has to be killed
      if no output occurs in specified timeout. Sometimes proc generates
      child process that may block its parent and for such cases
      proc_name refers to the name of child process.
      If proc_name is not specified, process name will be used to kill process.
    parser: A parser.
    timeout: A timeout(in seconds) to subprocess.stdout.readline method.
  """
  out = []
  if not proc_name:
    proc_name = psutil.Process(proc.pid).name()
  while True:
    # subprocess.stdout.readline() might be stuck from time to time
    # and tests fail because of TIMEOUT.
    # Try to fix the issue by adding timer-thread for `timeout` seconds
    # that will kill `frozen` running process if no new line is read
    # and will finish test attempt.
    # If new line appears in timeout, just cancel timer.
    try:
      timer = threading.Timer(timeout, terminate_process, [proc, proc_name])
      timer.start()
      line = proc.stdout.readline()
    finally:
      timer.cancel()
    if not line:
      break
    # |line| will be bytes on python3, and therefore must be decoded prior
    # to rstrip.
    if sys.version_info.major == 3:
      line = line.decode('utf-8')
    line = line.rstrip()
    out.append(line)
    if parser:
      parser.ProcessLine(line)
    LOGGER.info(line)
    sys.stdout.flush()

  if parser:
    parser.Finalize()
  LOGGER.debug('Finished print_process_output.')
  return out


def get_current_xcode_info():
  """Returns the current Xcode path, version, and build number.

  Returns:
    A dict with 'path', 'version', and 'build' keys.
      'path': The absolute path to the Xcode installation.
      'version': The Xcode version.
      'build': The Xcode build version.
  """
  try:
    out = subprocess.check_output(['xcodebuild',
                                   '-version']).decode('utf-8').splitlines()
    version, build_version = out[0].split(' ')[-1], out[1].split(' ')[-1]
    path = subprocess.check_output(['xcode-select',
                                    '--print-path']).decode('utf-8').rstrip()
  except subprocess.CalledProcessError:
    version = build_version = path = None

  return {
    'path': path,
    'version': version,
    'build': build_version,
  }


def init_test_result_defaults(is_eg_test=False):
  return {
      'version': 3,
      'path_delimiter': '/' if is_eg_test else '.',
      'seconds_since_epoch': int(time.time()),
      # This will be overwritten when the tests complete successfully.
      'interrupted': True,
      'num_failures_by_type': {},
      'tests': {}
  }


class TestRunner(object):
  """Base class containing common functionality."""

  def __init__(self, app_path, out_dir, **kwargs):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app to run.
      out_dir: Directory to emit test data into.
      (Following are potential args in **kwargs)
      env_vars: List of environment variables to pass to the test itself.
      readline_timeout: (int) Timeout to kill a test process when it doesn't
        have output (in seconds).
      repeat_count: Number of times to run each test case (passed to test app).
      retries: Number of times to retry failed test cases in test runner.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    app_path = os.path.abspath(app_path)
    if not os.path.exists(app_path):
      raise AppNotFoundError(app_path)

    xcode_info = get_current_xcode_info()
    LOGGER.info('Using Xcode version %s build %s at %s',
                 xcode_info['version'],
                 xcode_info['build'],
                 xcode_info['path'])

    if not os.path.exists(out_dir):
      os.makedirs(out_dir)

    self.app_name = os.path.splitext(os.path.split(app_path)[-1])[0]
    self.app_path = app_path
    self.cfbundleid = test_apps.get_bundle_id(app_path)
    self.env_vars = kwargs.get('env_vars') or []
    self.logs = collections.OrderedDict()
    self.out_dir = out_dir
    self.repeat_count = kwargs.get('repeat_count') or 1
    self.retries = kwargs.get('retries') or 0
    self.clones = kwargs.get('clones') or 1
    self.test_args = kwargs.get('test_args') or []
    self.test_cases = kwargs.get('test_cases') or []
    self.xctest_path = ''
    self.xctest = kwargs.get('xctest') or False
    self.readline_timeout = (
        kwargs.get('readline_timeout') or constants.READLINE_TIMEOUT)
    self.output_disabled_tests = kwargs.get('output_disabled_tests') or False

    self.test_results = init_test_result_defaults()

    if self.xctest:
      plugins_dir = os.path.join(self.app_path, 'PlugIns')
      if not os.path.exists(plugins_dir):
        raise PlugInsNotFoundError(plugins_dir)
      for plugin in os.listdir(plugins_dir):
        if plugin.endswith('.xctest'):
          self.xctest_path = os.path.join(plugins_dir, plugin)
      if not os.path.exists(self.xctest_path):
        raise XCTestPlugInNotFoundError(self.xctest_path)

  # TODO(crbug.com/40172018): Move this method to a utils class.
  @staticmethod
  def remove_proxy_settings():
    """removes any proxy settings which may remain from a previous run."""
    LOGGER.info('Removing any proxy settings.')
    network_services = subprocess.check_output(
        ['networksetup',
         '-listallnetworkservices']).decode('utf-8').strip().split('\n')
    if len(network_services) > 1:
      # We ignore the first line as it is a description of the command's output.
      network_services = network_services[1:]

      for service in network_services:
        # Disabled services have a '*' but calls should not include it
        if service.startswith('*'):
          service = service[1:]
        subprocess.check_call(
            ['networksetup', '-setsocksfirewallproxystate', service, 'off'])

  def get_launch_command(self, test_app, out_dir, destination, clones=1):
    """Returns the command that can be used to launch the test app.

    Args:
      test_app: An app that stores data about test required to run.
      out_dir: (str) A path for results.
      destination: (str) A destination of device/simulator.
      clones: (int) How many simulator clones the tests should be divided over.

    Returns:
      A list of strings forming the command to launch the test.
    """
    raise NotImplementedError

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    return os.environ.copy()

  def get_launch_test_app(self):
    """Returns the proper test_app for the run.

    Returns:
      An implementation of GTestsApp for the current run to execute.
    """
    raise NotImplementedError

  def start_proc(self, cmd):
    """Starts a process with cmd command and os.environ.

    Returns:
      An instance of process.
    """
    return subprocess.Popen(
        cmd,
        env=self.get_launch_env(),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )

  def shutdown_and_restart(self):
    """Restart a device or relaunch a simulator."""
    pass

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    raise NotImplementedError

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    raise NotImplementedError

  def retrieve_derived_data(self):
    """Retrieves the contents of DerivedData"""
    # DerivedData contains some logs inside workspace-specific directories.
    # Since we don't control the name of the workspace or project, most of
    # the directories are just called "temporary", making it hard to tell
    # which directory we need to retrieve. Instead we just delete the
    # entire contents of this directory before starting and return the
    # entire contents after the test is over.
    if os.path.exists(DERIVED_DATA):
      os.mkdir(os.path.join(self.out_dir, 'DerivedData'))
      derived_data = os.path.join(self.out_dir, 'DerivedData')
      for directory in os.listdir(DERIVED_DATA):
        LOGGER.info('Copying %s directory', directory)
        shutil.move(os.path.join(DERIVED_DATA, directory), derived_data)

  def wipe_derived_data(self):
    """Removes the contents of Xcode's DerivedData directory."""
    if os.path.exists(DERIVED_DATA):
      shutil.rmtree(DERIVED_DATA)
      os.mkdir(DERIVED_DATA)

  def process_xcresult_dir(self):
    """Copies artifacts & diagnostic logs, zips and removes .xcresult dir."""
    # .xcresult dir only exists when using Xcode 11+ and running as XCTest.
    if not xcode_util.using_xcode_11_or_higher() or not self.xctest:
      LOGGER.info('Skip processing xcresult directory.')

    xcresult_paths = []
    # Warning: This piece of code assumes .xcresult folder is directly under
    # self.out_dir. This is true for TestRunner subclasses in this file.
    # xcresult folder path is whatever passed in -resultBundlePath to xcodebuild
    # command appended with '.xcresult' suffix.
    for filename in os.listdir(self.out_dir):
      full_path = os.path.join(self.out_dir, filename)
      if full_path.endswith('.xcresult') and os.path.isdir(full_path):
        xcresult_paths.append(full_path)

    for xcresult in xcresult_paths:
      # This is what was passed in -resultBundlePath to xcodebuild command.
      result_bundle_path = os.path.splitext(xcresult)[0]
      XcodeLogParser.copy_artifacts(result_bundle_path)
      XcodeLogParser.export_diagnostic_data(result_bundle_path)
      # result_bundle_path is a symlink to xcresult directory.
      if os.path.islink(result_bundle_path):
        os.unlink(result_bundle_path)
      file_util.zip_and_remove_folder(xcresult)

  def run_tests(self, cmd=None):
    """Runs passed-in tests.

    Args:
      cmd: Command to run tests.

    Return:
      out: (list) List of strings of subprocess's output.
      returncode: (int) Return code of subprocess.
    """
    raise NotImplementedError

  def set_sigterm_handler(self, handler):
    """Sets the SIGTERM handler for the test runner.

    This is its own separate function so it can be mocked in tests.

    Args:
      handler: The handler to be called when a SIGTERM is caught

    Returns:
      The previous SIGTERM handler for the test runner.
    """
    LOGGER.debug('Setting sigterm handler.')
    return signal.signal(signal.SIGTERM, handler)

  def handle_sigterm(self, proc):
    """Handles a SIGTERM sent while a test command is executing.

    Will SIGKILL the currently executing test process, then
    attempt to exit gracefully.

    Args:
      proc: The currently executing test process.
    """
    LOGGER.warning('Sigterm caught during test run. Killing test process.')
    proc.kill()

  def _run(self, cmd, clones=1):
    """Runs the specified command, parsing GTest output.

    Args:
      cmd: List of strings forming the command to run.

    Returns:
      TestResult.ResultCollection() object.
    """
    parser = gtest_utils.GTestLogParser()

    # TODO(crbug.com/41370857): Implement test sharding for unit tests.
    # TODO(crbug.com/41370858): Use thread pool for DeviceTestRunner as well.
    proc = self.start_proc(cmd)
    old_handler = self.set_sigterm_handler(
        lambda _signum, _frame: self.handle_sigterm(proc))
    print_process_output(
        proc, 'xcodebuild', parser, timeout=self.readline_timeout)
    LOGGER.info('Waiting for test process to terminate.')
    proc.wait()
    LOGGER.info('Test process terminated.')
    self.set_sigterm_handler(old_handler)
    sys.stdout.flush()
    LOGGER.debug('Stdout flushed after test process.')
    returncode = proc.returncode

    LOGGER.info('%s returned %s\n', cmd[0], returncode)

    LOGGER.info('Populating test location info for test results...')
    if isinstance(self, SimulatorTestRunner):
      # TODO(crbug.com/40134137): currently we have some tests suites that are
      # written in ios_internal, so not all test repos are public. We should
      # figure out a way to identify test repo info depending on the test suite.
      parser.ParseAndPopulateTestResultLocations(DEFAULT_TEST_REPO,
                                                 self.output_disabled_tests)
    elif isinstance(self, DeviceTestRunner):
      # Pull the file from device first before parsing.
      if (parser.compiled_tests_file_path != None):
        LOGGER.info('Pulling test location file from iOS device Documents...')
        file_name = os.path.split(parser.compiled_tests_file_path)[1]
        pull_cmd = [
            'idevicefs', '--udid', self.udid, 'pull',
            '@%s/Documents/%s' % (self.cfbundleid, file_name), self.out_dir
        ]
        print_process_output(self.start_proc(pull_cmd))
        host_tests_file_path = os.path.join(self.out_dir, file_name)
        parser.ParseAndPopulateTestResultLocations(DEFAULT_TEST_REPO,
                                                   self.output_disabled_tests,
                                                   host_tests_file_path)
      else:
        LOGGER.warning('No compiled test files found in documents dir...')

    else:
      LOGGER.warning('Test location reporting is not yet supported on %s',
                     type(self))

    return parser.GetResultCollection()

  def launch(self):
    """Launches the test app."""
    self.set_up()
    # The overall ResultCorrection object holding all runs of all tests in the
    # runner run. It will be updated with each test application launch.
    overall_result = ResultCollection()
    destination = 'id=%s' % self.udid
    test_app = self.get_launch_test_app()
    out_dir = os.path.join(self.out_dir, 'TestResults')
    cmd = self.get_launch_command(test_app, out_dir, destination, self.clones)
    try:
      result = self._run(cmd=cmd, clones=self.clones or 1)
      if (result.crashed and not result.spawning_test_launcher and
          not result.crashed_tests()):
        # If the app crashed but not during any particular test case, assume
        # it crashed on startup. Try one more time.
        self.shutdown_and_restart()
        LOGGER.warning('Crashed on startup, retrying...\n')
        out_dir = os.path.join(self.out_dir, 'retry_after_crash_on_startup')
        cmd = self.get_launch_command(test_app, out_dir, destination,
                                      self.clones)
        result = self._run(cmd)

      result.report_to_result_sink()

      if (result.crashed and not result.spawning_test_launcher and
          not result.crashed_tests()):
        raise AppLaunchError

      overall_result.add_result_collection(result)

      try:
        while (result.crashed and not result.spawning_test_launcher and
               result.crashed_tests()):
          # If the app crashes during a specific test case, then resume at the
          # next test case. This is achieved by filtering out every test case
          # which has already run.
          LOGGER.warning('Crashed during %s, resuming...\n',
                         list(result.crashed_tests()))
          test_app.excluded_tests = list(overall_result.all_test_names())
          # Changing test filter will change selected gtests in this shard.
          # Thus, sharding env vars have to be cleared to ensure needed tests
          # are run. This means there might be duplicate same tests across
          # the shards.
          test_app.remove_gtest_sharding_env_vars()
          retry_out_dir = os.path.join(
              self.out_dir, 'retry_after_crash_%d' % int(time.time()))
          result = self._run(
              self.get_launch_command(test_app, retry_out_dir, destination))
          result.report_to_result_sink()
          # Only keep the last crash status in crash retries in overall crash
          # status.
          overall_result.add_result_collection(result, overwrite_crash=True)

      except OSError as e:
        if e.errno == errno.E2BIG:
          LOGGER.error('Too many test cases to resume.')
        else:
          raise

      # Retry failed test cases.
      test_app.excluded_tests = []
      never_expected_tests = overall_result.never_expected_tests()
      if (self.retries and not result.spawning_test_launcher and
          never_expected_tests):
        LOGGER.warning('%s tests failed and will be retried.\n',
                       len(never_expected_tests))
        for i in range(self.retries):
          tests_to_retry = list(overall_result.never_expected_tests())
          for test in tests_to_retry:
            LOGGER.info('Retry #%s for %s.\n', i + 1, test)
            test_app.included_tests = [test]
            # Changing test filter will change selected gtests in this shard.
            # Thus, sharding env vars have to be cleared to ensure the test
            # runs when it's the only test in gtest_filter.
            test_app.remove_gtest_sharding_env_vars()
            test_retry_sub_dir = '%s_retry_%d' % (test.replace('/', '_'), i)
            retry_out_dir = os.path.join(self.out_dir, test_retry_sub_dir)
            retry_result = self._run(
                self.get_launch_command(test_app, retry_out_dir, destination))

            if not retry_result.all_test_names():
              retry_result.add_test_result(
                  TestResult(
                      test,
                      TestStatus.SKIP,
                      test_log='In single test retry, result of this test '
                      'didn\'t appear in log.'))
            retry_result.report_to_result_sink()
            # No unknown tests might be skipped so do not change
            # |overall_result|'s crash status.
            overall_result.add_result_collection(
                retry_result, ignore_crash=True)

      interrupted = overall_result.crashed

      if interrupted:
        overall_result.set_crashed_with_prefix(
            crash_message_prefix_line='Test application crashed when running '
            'tests which might have caused some tests never ran or finished.')

      self.test_results = overall_result.standard_json_output()
      self.logs.update(overall_result.test_runner_logs())

      return not overall_result.never_expected_tests() and not interrupted
    finally:
      self.tear_down()


class SimulatorTestRunner(TestRunner):
  """Class for running tests on iossim."""

  def __init__(self, app_path, iossim_path, platform, version, out_dir,
               **kwargs):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app or .ipa to run.
      iossim_path: Path to the compiled iossim binary to use.
      platform: Name of the platform to simulate. Supported values can be found
        by running "iossim -l". e.g. "iPhone 5s", "iPad Retina".
      version: Version of iOS the platform should be running. Supported values
        can be found by running "iossim -l". e.g. "9.3", "8.2", "7.1".
      out_dir: Directory to emit test data into.
      (Following are potential args in **kwargs)
      env_vars: List of environment variables to pass to the test itself.
      repeat_count: Number of times to run each test case (passed to test app).
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      use_clang_coverage: Whether code coverage is enabled in this run.
      wpr_tools_path: Path to pre-installed WPR-related tools
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(SimulatorTestRunner, self).__init__(app_path, out_dir, **kwargs)

    iossim_path = os.path.abspath(iossim_path)
    if not os.path.exists(iossim_path):
      raise SimulatorNotFoundError(iossim_path)

    self.homedir = ''
    self.iossim_path = iossim_path
    self.platform = platform
    self.start_time = None
    self.version = version
    self.clones = kwargs.get('clones') or 1
    self.udid = iossim_util.get_simulator(self.platform, self.version)
    self.use_clang_coverage = kwargs.get('use_clang_coverage') or False

  @staticmethod
  def kill_simulators():
    """Kills all running simulators."""
    try:
      LOGGER.info('Killing simulators.')
      subprocess.check_call([
          'pkill',
          '-9',
          '-x',
          # The simulator's name varies by Xcode version.
          'com.apple.CoreSimulator.CoreSimulatorService', # crbug.com/684305
          'iPhone Simulator', # Xcode 5
          'iOS Simulator', # Xcode 6
          'Simulator', # Xcode 7+
          'simctl', # https://crbug.com/637429
          'xcodebuild', # https://crbug.com/684305
      ])
      # If a signal was sent, wait for the simulators to actually be killed.
      time.sleep(5)
    except subprocess.CalledProcessError as e:
      if e.returncode != 1:
        # Ignore a 1 exit code (which means there were no simulators to kill).
        raise

  def wipe_simulator(self):
    """Wipes the simulator."""
    iossim_util.wipe_simulator_by_udid(self.udid)

  def disable_hw_keyboard(self):
    """Disables hardware keyboard input."""
    iossim_util.disable_hardware_keyboard(self.udid)

  def get_home_directory(self):
    """Returns the simulator's home directory."""
    return iossim_util.get_home_directory(self.platform, self.version)

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.remove_proxy_settings()
    self.kill_simulators()
    self.wipe_simulator()
    self.wipe_derived_data()
    self.disable_hw_keyboard()
    self.homedir = self.get_home_directory()
    # Crash reports have a timestamp in their file name, formatted as
    # YYYY-MM-DD-HHMMSS. Save the current time in the same format so
    # we can compare and fetch crash reports from this run later on.
    self.start_time = time.strftime('%Y-%m-%d-%H%M%S', time.localtime())

  def extract_test_data(self):
    """Extracts data emitted by the test."""
    if hasattr(self, 'use_clang_coverage') and self.use_clang_coverage:
      file_util.move_raw_coverage_data(self.udid, self.out_dir)

    # Find the Documents directory of the test app. The app directory names
    # don't correspond with any known information, so we have to examine them
    # all until we find one with a matching CFBundleIdentifier.
    apps_dir = os.path.join(
        self.homedir, 'Containers', 'Data', 'Application')
    if os.path.exists(apps_dir):
      for appid_dir in os.listdir(apps_dir):
        docs_dir = os.path.join(apps_dir, appid_dir, 'Documents')
        metadata_plist = os.path.join(
            apps_dir,
            appid_dir,
            '.com.apple.mobile_container_manager.metadata.plist',
        )
        if os.path.exists(docs_dir) and os.path.exists(metadata_plist):
          cfbundleid = subprocess.check_output([
              '/usr/libexec/PlistBuddy',
              '-c',
              'Print:MCMMetadataIdentifier',
              metadata_plist,
          ]).decode('utf-8').rstrip()
          if cfbundleid == self.cfbundleid:
            shutil.copytree(docs_dir, os.path.join(self.out_dir, 'Documents'))
            return

  def retrieve_crash_reports(self):
    """Retrieves crash reports produced by the test."""
    # A crash report's naming scheme is [app]_[timestamp]_[hostname].crash.
    # e.g. net_unittests_2014-05-13-15-0900_vm1-a1.crash.
    crash_reports_dir = os.path.expanduser(os.path.join(
        '~', 'Library', 'Logs', 'DiagnosticReports'))

    if not os.path.exists(crash_reports_dir):
      return

    for crash_report in os.listdir(crash_reports_dir):
      report_name, ext = os.path.splitext(crash_report)
      if report_name.startswith(self.app_name) and ext == '.crash':
        report_time = report_name[len(self.app_name) + 1:].split('_')[0]

        # The timestamp format in a crash report is big-endian and therefore
        # a straight string comparison works.
        if report_time > self.start_time:
          with open(os.path.join(crash_reports_dir, crash_report)) as f:
            self.logs['crash report (%s)' % report_time] = (
                f.read().splitlines())

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    LOGGER.debug('Extracting test data.')
    self.extract_test_data()
    LOGGER.debug('Retrieving crash reports.')
    self.retrieve_crash_reports()
    LOGGER.debug('Retrieving derived data.')
    self.retrieve_derived_data()
    LOGGER.debug('Processing xcresult folder.')
    self.process_xcresult_dir()
    LOGGER.debug('Killing simulators.')
    self.kill_simulators()
    LOGGER.debug('Wiping simulator.')
    self.wipe_simulator()
    LOGGER.debug('Deleting simulator.')
    self.deleteSimulator(self.udid)
    if os.path.exists(self.homedir):
      shutil.rmtree(self.homedir, ignore_errors=True)
      self.homedir = ''
    LOGGER.debug('End of tear_down.')

  def run_tests(self, cmd):
    """Runs passed-in tests. Builds a command and create a simulator to
      run tests.
    Args:
      cmd: A running command.

    Return:
      out: (list) List of strings of subprocess's output.
      returncode: (int) Return code of subprocess.
    """
    proc = self.start_proc(cmd)
    out = print_process_output(
        proc,
        'xcodebuild',
        xctest_utils.XCTestLogParser(),
        timeout=self.readline_timeout)
    self.deleteSimulator(self.udid)
    return (out, proc.returncode)

  def getSimulator(self):
    """Gets a simulator or creates a new one by device types and runtimes.
      Returns the udid for the created simulator instance.

    Returns:
      An udid of a simulator device.
    """
    return iossim_util.get_simulator(self.platform, self.version)

  def deleteSimulator(self, udid=None):
    """Removes dynamically created simulator devices."""
    if udid:
      iossim_util.delete_simulator_by_udid(udid)

  def get_launch_command(self, test_app, out_dir, destination, clones=1):
    """Returns the command that can be used to launch the test app.

    Args:
      test_app: An app that stores data about test required to run.
      out_dir: (str) A path for results.
      destination: (str) A destination of device/simulator.
      clones: (int) How many simulator clones the tests should be divided over.

    Returns:
      A list of strings forming the command to launch the test.
    """
    return test_app.command(out_dir, destination, clones)

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(SimulatorTestRunner, self).get_launch_env()
    if self.xctest:
      env['NSUnbufferedIO'] = 'YES'
    return env

  def get_launch_test_app(self):
    """Returns the proper test_app for the run.

    Returns:
      A SimulatorXCTestUnitTestsApp for the current run to execute.
    """
    # Non iOS Chrome users have unit tests not built with XCTest.
    if not self.xctest:
      return test_apps.GTestsApp(
          self.app_path,
          included_tests=self.test_cases,
          env_vars=self.env_vars,
          repeat_count=self.repeat_count,
          test_args=self.test_args)

    return test_apps.SimulatorXCTestUnitTestsApp(
        self.app_path,
        included_tests=self.test_cases,
        env_vars=self.env_vars,
        repeat_count=self.repeat_count,
        test_args=self.test_args)


class DeviceTestRunner(TestRunner):
  """Class for running tests on devices."""

  def __init__(self, app_path, out_dir, **kwargs):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app to run.
      out_dir: Directory to emit test data into.
      (Following are potential args in **kwargs)
      env_vars: List of environment variables to pass to the test itself.
      repeat_count: Number of times to run each test case (passed to test app).
      restart: Whether or not restart device when test app crashes on startup.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    super(DeviceTestRunner, self).__init__(app_path, out_dir, **kwargs)

    self.udid = subprocess.check_output(['idevice_id',
                                         '--list']).decode('utf-8').rstrip()
    if len(self.udid.splitlines()) != 1:
      raise DeviceDetectionError(self.udid)

    self.restart = kwargs.get('restart') or False

  def uninstall_apps(self):
    """Uninstalls all apps found on the device."""
    for app in self.get_installed_packages():
      cmd = ['ideviceinstaller', '--udid', self.udid, '--uninstall', app]
      print_process_output(self.start_proc(cmd))

  def install_app(self):
    """Installs the app."""
    cmd = ['ideviceinstaller', '--udid', self.udid, '--install', self.app_path]
    print_process_output(self.start_proc(cmd))

  def get_installed_packages(self):
    """Gets a list of installed packages on a device.

    Returns:
      A list of installed packages on a device.
    """
    cmd = ['idevicefs', '--udid', self.udid, 'ls', '@']
    return print_process_output(self.start_proc(cmd))

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.uninstall_apps()
    self.wipe_derived_data()
    self.install_app()
    self.restart_usbmuxd()

  def extract_test_data(self):
    """Extracts data emitted by the test."""
    cmd = [
        'idevicefs',
        '--udid', self.udid,
        'pull',
        '@%s/Documents' % self.cfbundleid,
        os.path.join(self.out_dir, 'Documents'),
    ]
    try:
      print_process_output(self.start_proc(cmd))
    except subprocess.CalledProcessError:
      raise TestDataExtractionError()

  def shutdown_and_restart(self):
    """Restart the device, wait for two minutes."""
    # TODO(crbug.com/41341969): swarming bot ios 11 devices turn to be
    # unavailable in a few hours unexpectedly, which is assumed as an ios beta
    # issue. Should remove this method once the bug is fixed.
    if self.restart:
      LOGGER.info('Restarting device, wait for two minutes.')
      try:
        subprocess.check_call(
          ['idevicediagnostics', 'restart', '--udid', self.udid])
      except subprocess.CalledProcessError:
        raise DeviceRestartError()
      time.sleep(120)

  def retrieve_crash_reports(self):
    """Retrieves crash reports produced by the test."""
    logs_dir = os.path.join(self.out_dir, 'Logs')
    os.mkdir(logs_dir)
    cmd = [
        'idevicecrashreport',
        '--extract',
        '--udid', self.udid,
        logs_dir,
    ]
    try:
      print_process_output(self.start_proc(cmd))
    except subprocess.CalledProcessError:
      # TODO(crbug.com/41380784): Raise the exception when the bug is fixed.
      LOGGER.warning('Failed to retrieve crash reports from device.')

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    self.retrieve_derived_data()
    self.extract_test_data()
    self.process_xcresult_dir()
    self.retrieve_crash_reports()
    self.uninstall_apps()

  def get_launch_command(self, test_app, out_dir, destination, clones=1):
    """Returns the command that can be used to launch the test app.

    Args:
      test_app: An app that stores data about test required to run.
      out_dir: (str) A path for results.
      destination: (str) A destination of device/simulator.
      clones: (int) How many simulator clones the tests should be divided over.

    Returns:
      A list of strings forming the command to launch the test.
    """
    if self.xctest:
      return test_app.command(out_dir, destination, clones)

    cmd = [
      'idevice-app-runner',
      '--udid', self.udid,
      '--start', self.cfbundleid,
    ]
    args = []

    if test_app.included_tests or test_app.excluded_tests:
      gtest_filter = test_apps.get_gtest_filter(test_app.included_tests,
                                                test_app.excluded_tests)
      args.append('--gtest_filter=%s' % gtest_filter)

    for env_var in self.env_vars:
      cmd.extend(['-D', env_var])

    if args or self.test_args:
      cmd.append('--args')
      cmd.extend(self.test_args)
      cmd.extend(args)

    return cmd

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(DeviceTestRunner, self).get_launch_env()
    if self.xctest:
      env['NSUnbufferedIO'] = 'YES'
      # e.g. ios_web_shell_egtests
      env['APP_TARGET_NAME'] = os.path.splitext(
          os.path.basename(self.app_path))[0]
      # e.g. ios_web_shell_egtests_module
      env['TEST_TARGET_NAME'] = env['APP_TARGET_NAME'] + '_module'
    return env

  def get_launch_test_app(self):
    """Returns the proper test_app for the run.

    Returns:
      A DeviceXCTestUnitTestsApp  for the current run to execute.
    """
    # Non iOS Chrome users have unit tests not built with XCTest.
    if not self.xctest:
      return test_apps.GTestsApp(
          self.app_path,
          included_tests=self.test_cases,
          env_vars=self.env_vars,
          repeat_count=self.repeat_count,
          test_args=self.test_args)

    return test_apps.DeviceXCTestUnitTestsApp(
        self.app_path,
        included_tests=self.test_cases,
        env_vars=self.env_vars,
        repeat_count=self.repeat_count,
        test_args=self.test_args)

  # TODO(crbug.com/40277601): there's a bug in Xcode 15 such that the devices
  # will get disconnected from Xcode after a reboot. We should revisit this
  # later to see if Apple will resolve this issue. Moreover, if the issue is
  # not resolved, we should aim to add some restrictions to this call such
  # that stop_usbmuxd is not called every single time.
  def restart_usbmuxd(self):
    if xcode_util.using_xcode_15_or_higher():
      LOGGER.warning(
          "Restarting usbmuxd to ensure device is re-paired to Xcode...")
      try:
        mac_util.kill_usbmuxd()
      except subprocess.CalledProcessError as e:
        logging.exception('Unable to restart usbmuxd:')
        logging.error(e)
