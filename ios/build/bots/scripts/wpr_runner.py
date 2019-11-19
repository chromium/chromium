# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Test runner for running tests using xcodebuild."""

import glob
import logging
import os
import signal
import shutil
import subprocess
import sys

import gtest_utils
import test_runner
import xcodebuild_runner
import xctest_utils

LOGGER = logging.getLogger(__name__)


class CertPathNotFoundError(test_runner.TestRunnerError):
  """The certificate path was not found."""

  def __init__(self, replay_path):
    super(CertPathNotFoundError,
          self).__init__('Cert path does not exist: %s' % replay_path)


class ReplayPathNotFoundError(test_runner.TestRunnerError):
  """The replay path was not found."""

  def __init__(self, replay_path):
    super(ReplayPathNotFoundError,
          self).__init__('Replay path does not exist: %s' % replay_path)


class WprToolsNotFoundError(test_runner.TestRunnerError):
  """wpr_tools_path is not specified."""

  def __init__(self, wpr_tools_path):
    super(WprToolsNotFoundError, self).__init__(
        'wpr_tools_path is not specified or not found: "%s"' % wpr_tools_path)


class WprProxySimulatorTestRunner(test_runner.SimulatorTestRunner):
  """Class for running simulator tests with WPR against saved website replays"""

  def __init__(
      self,
      app_path,
      host_app_path,
      iossim_path,
      replay_path,
      platform,
      version,
      wpr_tools_path,
      xcode_build_version,
      out_dir,
      env_vars=None,
      mac_toolchain='',
      retries=None,
      shards=None,
      test_args=None,
      test_cases=None,
      xcode_path='',
      xctest=False,
  ):
    """Initializes a new instance of this class.

    Args:
      app_path: Path to the compiled .app or .ipa to run.
      host_app_path: A path to the host app for EG2.
      iossim_path: Path to the compiled iossim binary to use.
      replay_path: Path to the folder where WPR replay and recipe files live.
      platform: Name of the platform to simulate. Supported values can be found
        by running "iossim -l". e.g. "iPhone 5s", "iPad Retina".
      version: Version of iOS the platform should be running. Supported values
        can be found by running "iossim -l". e.g. "9.3", "8.2", "7.1".
      wpr_tools_path: Path to pre-installed (from CIPD) WPR-related tools
      xcode_build_version: Xcode build version to install before running tests.
      out_dir: Directory to emit test data into.
      env_vars: List of environment variables to pass to the test itself.
      mac_toolchain: Command to run `mac_toolchain` tool.
      retries: Number of times to retry failed test cases.
      test_args: List of strings to pass as arguments to the test when
        launching.
      test_cases: List of tests to be included in the test run. None or [] to
        include all tests.
      xcode_path: Path to Xcode.app folder where its contents will be installed.
      xctest: Whether or not this is an XCTest.

    Raises:
      AppNotFoundError: If the given app does not exist.
      ReplayPathNotFoundError: If the replay path was not found.
      WprToolsNotFoundError: If wpr_tools_path is not specified.
    """
    super(WprProxySimulatorTestRunner, self).__init__(
        app_path,
        iossim_path,
        platform,
        version,
        xcode_build_version,
        out_dir,
        env_vars=env_vars,
        mac_toolchain=mac_toolchain,
        retries=retries,
        shards=shards,
        test_args=test_args,
        test_cases=test_cases,
        wpr_tools_path=wpr_tools_path,
        xcode_path=xcode_path,
        xctest=xctest,
    )
    self.host_app_path = None
    if host_app_path is not None and host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
      if not os.path.exists(self.host_app_path):
        raise test_runner.AppNotFoundError(self.host_app_path)

    self.use_trusted_cert = True

    self.test_attempt_count = {}

    replay_path = os.path.abspath(replay_path)
    if not os.path.exists(replay_path):
      raise ReplayPathNotFoundError(replay_path)
    self.replay_path = replay_path

    if not os.path.exists(wpr_tools_path):
      raise WprToolsNotFoundError(wpr_tools_path)

    self.proxy_process = None
    self.wprgo_process = None

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    super(WprProxySimulatorTestRunner, self).set_up()
    self.proxy_start()

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    super(WprProxySimulatorTestRunner, self).tear_down()
    self.proxy_stop()
    self.wprgo_stop()

  def get_wpr_test_command(self, recipe_path, test_name):
    """Creates xcodebuild commands for running a wpr test per recipe_path.

    Args:
      recipe_path: (str) Path to wpr recipe file.
      test_name: (str) Test name(format: ios_website) of this wpr test.

    Returns:
      Xcodebuild command to run in the format of a list of str.
    """
    wpr_test_args = [
        '--enable-features=AutofillShowTypePredictions',
        '-autofillautomation=%s' % recipe_path,
    ]
    wpr_egtests_app = xcodebuild_runner.EgtestsApp(
        self.app_path,
        included_tests=["AutofillAutomationTestCase"],
        env_vars=self.env_vars,
        test_args=wpr_test_args,
        host_app_path=self.host_app_path)

    self.test_attempt_count[test_name] = self.test_attempt_count.get(
        test_name, 0) + 1

    destination = 'platform=iOS Simulator,OS=%s,name=%s' % (self.version,
                                                            self.platform)
    destination_folder = '%s %s %s attempt#%s' % (
        self.version, self.platform, test_name,
        self.test_attempt_count[test_name])
    out_dir = os.path.join(self.out_dir, destination_folder)

    launch_command = xcodebuild_runner.LaunchCommand(
        wpr_egtests_app, destination, self.shards, self.retries)
    return launch_command.command(wpr_egtests_app, out_dir, destination,
                                  self.shards)

  def get_launch_env(self):
    """Returns a dict of environment variables to use to launch the test app.

    Returns:
      A dict of environment variables.
    """
    env = super(test_runner.SimulatorTestRunner, self).get_launch_env()
    env['NSUnbufferedIO'] = 'YES'
    return env

  def run_wpr_test(self, udid, test_name, recipe_path, replay_path):
    """Runs a single WPR test.

    Args:
      udid: UDID for the simulator to run the test on
      test_name: Test name(format: ios_website) of this wpr test.
      recipe_path: Path to the recipe file (i.e. ios_costco.test)
      replay_path: Path to the replay file (i.e. ios_costco)

    Returns
      [parser, return code from test] where
      parser: a XCTest or GTestLogParser which has processed all
        the output from the test
    """
    LOGGER.info('Running test for recipe %s', recipe_path)
    self.wprgo_start(replay_path)

    # TODO(crbug.com/881096): Consider reusing get_launch_command
    #  and adding the autofillautomation flag to it

    # TODO(crbug.com/881096): We only run AutofillAutomationTestCase
    #  as we have other unit tests in the suite which are not related
    #  to testing website recipe/replays. We should consider moving
    #  one or the other to a different suite.

    # For the website replay test suite, we need to pass in a single
    # recipe at a time, with flags "autofillautomation={recipe_path}",
    # "--enable-features=AutofillShowTypePredictions". The args are written in
    # xctestrun file, which is produced through EgtestsApp and LaunchCommand
    # defined in xcodebuild_runner.
    wpr_test_cmd = self.get_wpr_test_command(recipe_path, test_name)

    proc = self.start_proc(wpr_test_cmd)
    old_handler = self.set_sigterm_handler(
        lambda _signum, _frame: self.handle_sigterm(proc))

    if self.xctest_path:
      parser = xctest_utils.XCTestLogParser()
    else:
      parser = gtest_utils.GTestLogParser()

    test_runner.print_process_output(proc, 'xcodebuild', parser)

    proc.wait()
    self.set_sigterm_handler(old_handler)
    sys.stdout.flush()

    self.wprgo_stop()

    return parser, proc.returncode

  def should_run_wpr_test(self, recipe_name, test_filter, invert):
    """Returns whether the WPR test should be run, given the filters.

      Args:
        recipe_name: Filename of the recipe to run (i.e. 'ios_costco')
        test_filter: List of tests to run. If recipe_name is found as
          a substring of any of these, then the filter is matched.
        invert: If true, run tests that are not matched by the filter.

      Returns:
        True if the test should be run.
    """
    # If the matching replay for the recipe doesn't exist, don't run it
    replay_path = '{}/{}'.format(self.replay_path, recipe_name)
    if not os.path.isfile(replay_path):
      LOGGER.error('No matching replay file for recipe %s', recipe_name)
      return False

    # if there is no filter, then run tests
    if not test_filter:
      return True

    test_matched_filter = False
    for filter_name in test_filter:
      if recipe_name in filter_name:
        test_matched_filter = True

    return test_matched_filter != invert

  def copy_trusted_certificate(self):
    """Copies a TrustStore file with a trusted HTTPS certificate into all sims.

    This allows the simulators to access HTTPS webpages served through WprGo.

    Raises:
      WprToolsNotFoundError: If wpr_tools_path is not specified.

    """

    if not os.path.exists(self.wpr_tools_path):
      raise WprToolsNotFoundError(self.wpr_tools_path)
    cert_path = "{}/TrustStore_trust.sqlite3".format(self.wpr_tools_path)

    if not os.path.exists(cert_path):
      raise CertPathNotFoundError(cert_path)

    trust_stores = glob.glob(
        '{}/Library/Developer/CoreSimulator/Devices/*/data/Library'.format(
            os.path.expanduser('~')))
    for trust_store in trust_stores:
      LOGGER.info('Copying TrustStore to %s', trust_store)
      trust_store_file_path = os.path.join(trust_store,
                                           'Keychains/TrustStore.sqlite3')
      if os.path.isfile(trust_store_file_path):
        os.remove(trust_store_file_path)
      trust_store_dir_path = os.path.join(trust_store, 'Keychains')
      if not os.path.exists(trust_store_dir_path):
        os.makedirs(trust_store_dir_path)
      shutil.copy(cert_path, trust_store_file_path)

  def _run(self, cmd, shards=1):
    """Runs the specified command, parsing GTest output.

    Args:
      cmd: List of strings forming the command to run.
      NOTE: in the case of WprProxySimulatorTestRunner, cmd
        is a dict forming the configuration for the test (including
        filter rules), and not indicative of the actual command
        we build and execute in _run.

    Returns:
      GTestResult instance.

    Raises:
      ShardingDisabledError: If shards > 1 as currently sharding is not
        supported.
      SystemAlertPresentError: If system alert is shown on the device.
    """
    result = gtest_utils.GTestResult(cmd)
    completed_without_failure = True
    total_returncode = 0
    if shards > 1:
      # TODO(crbug.com/881096): reimplement sharding in the future
      raise test_runner.ShardingDisabledError()

    # TODO(crbug.com/812705): Implement test sharding for unit tests.
    # TODO(crbug.com/812712): Use thread pool for DeviceTestRunner as well.

    # Create a simulator for these tests, and prepare it with the
    # certificate needed for HTTPS proxying.
    udid = self.getSimulator()

    self.copy_trusted_certificate()

    for recipe_path in glob.glob('{}/*.test'.format(self.replay_path)):
      base_name = os.path.basename(recipe_path)
      test_name = os.path.splitext(base_name)[0]
      replay_path = '{}/{}'.format(self.replay_path, test_name)

      if self.should_run_wpr_test(test_name, cmd['test_filter'], cmd['invert']):

        parser, returncode = self.run_wpr_test(udid, test_name, recipe_path,
                                               replay_path)

        # If this test fails, immediately rerun it to see if it deflakes.
        # We simply overwrite the first result with the second.
        if parser.FailedTests(include_flaky=True):
          parser, returncode = self.run_wpr_test(udid, test_name, recipe_path,
                                                 replay_path)

        for test in parser.FailedTests(include_flaky=True):
          # All test names will be the same since we re-run the same suite;
          # therefore, to differentiate the results, we append the recipe
          # name to the test suite.
          testWithRecipeName = "{}.{}".format(base_name, test)

          # Test cases are named as <test group>.<test case>. If the test case
          # is prefixed w/"FLAKY_", it should be reported as flaked not failed
          if '.' in test and test.split('.', 1)[1].startswith('FLAKY_'):
            result.flaked_tests[testWithRecipeName] = parser.FailureDescription(
                test)
          else:
            result.failed_tests[testWithRecipeName] = parser.FailureDescription(
                test)

        for test in parser.PassedTests(include_flaky=True):
          testWithRecipeName = "{}.{}".format(base_name, test)
          result.passed_tests.extend([testWithRecipeName])

        # Check for runtime errors.
        if self.xctest_path and parser.SystemAlertPresent():
          raise test_runner.SystemAlertPresentError()
        if returncode != 0:
          total_returncode = returncode
        if not parser.CompletedWithoutFailure():
          completed_without_failure = False
        LOGGER.info('%s test returned %s\n', recipe_path, returncode)

    self.deleteSimulator(udid)

    # iossim can return 5 if it exits noncleanly even if all tests passed.
    # Therefore we cannot rely on process exit code to determine success.

    # NOTE: total_returncode is 0 OR the last non-zero return code from a test.
    result.finalize(total_returncode, completed_without_failure)
    return result

  def get_launch_command(self, test_filter=[], invert=False):
    """Returns a config dict for the test, instead of the real launch command.
    Normally this is passed into _run as the command it should use, but since
    the WPR runner builds its own cmd, we use this to configure the function.

    Args:
      test_filter: List of test cases to filter.
      invert: Whether to invert the filter or not. Inverted, the filter will
      match everything except the given test cases.

    Returns:
      A dict forming the configuration for the test.
    """

    test_config = {}
    test_config['invert'] = invert
    test_config['test_filter'] = test_filter
    return test_config

  def proxy_start(self):
    """Starts tsproxy and routes the machine's traffic through tsproxy."""

    # Stops any straggling instances of WPRgo that may hog ports 8080/8081
    subprocess.check_call('lsof -ti:8080 | xargs kill -9', shell=True)
    subprocess.check_call('lsof -ti:8081| xargs kill -9', shell=True)

    # We route all network adapters through the proxy, since it is easier than
    # determining which network adapter is being used currently.
    network_services = subprocess.check_output(
        ['networksetup', '-listallnetworkservices']).strip().split('\n')
    if len(network_services) > 1:
      # We ignore the first line as it is a description of the command's output.
      network_services = network_services[1:]

      for service in network_services:
        subprocess.check_call(
            ['networksetup', '-setsocksfirewallproxystate', service, 'on'])
        subprocess.check_call([
            'networksetup', '-setsocksfirewallproxy', service, '127.0.0.1',
            '1080'
        ])

    self.proxy_process = subprocess.Popen(
        [
            'python', 'tsproxy.py', '--port=1080', '--desthost=127.0.0.1',
            '--mapports=443:8081,*:8080'
        ],
        cwd='{}/tsproxy'.format(self.wpr_tools_path),
        env=self.get_launch_env(),
        stdout=open(os.path.join(self.out_dir, 'stdout_proxy.txt'), 'wb+'),
        stderr=subprocess.STDOUT,
    )

  def proxy_stop(self):
    """Stops tsproxy and disables the machine's proxy settings."""
    if self.proxy_process is not None:
      os.kill(self.proxy_process.pid, signal.SIGINT)

    network_services = subprocess.check_output(
        ['networksetup', '-listallnetworkservices']).strip().split('\n')
    if len(network_services) > 1:
      # We ignore the first line as it is a description of the command's output.
      network_services = network_services[1:]

      for service in network_services:
        subprocess.check_call(
            ['networksetup', '-setsocksfirewallproxystate', service, 'off'])

  def wprgo_start(self, replay_path):
    """Starts WprGo serving the specified replay file.

      Args:
        replay_path: Path to the WprGo website replay to use.
    """
    self.wprgo_process = subprocess.Popen(
        [
            './wpr', 'replay', '--http_port=8080', '--https_port=8081',
            replay_path
        ],
        cwd='{}/web_page_replay_go/'.format(self.wpr_tools_path),
        env=self.get_launch_env(),
        stdout=open(os.path.join(self.out_dir, 'stdout_wprgo.txt'), 'wb+'),
        stderr=subprocess.STDOUT,
    )

  def wprgo_stop(self):
    """Stops serving website replays using WprGo."""
    if self.wprgo_process is not None:
      os.kill(self.wprgo_process.pid, signal.SIGINT)
