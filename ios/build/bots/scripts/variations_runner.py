# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runner class for variations smoke tests."""

from datetime import datetime
import logging
import os
import subprocess
import sys

import iossim_util
import test_apps
import test_runner
from test_result_util import ResultCollection, TestResult, TestStatus
from xcodebuild_runner import SimulatorParallelTestRunner
from xcode_log_parser import XcodeLogParser

_THIS_DIR = os.path.dirname(os.path.abspath(__file__))
_SRC_DIR = os.path.join(_THIS_DIR, os.path.pardir, os.path.pardir,
                        os.path.pardir, os.path.pardir)
_VARIATIONS_SMOKE_TEST_DIR = os.path.join(_SRC_DIR, 'testing', 'scripts')
sys.path.insert(0, _VARIATIONS_SMOKE_TEST_DIR)

import variations_seed_access_helper as seed_helper


# Constants around the variation keys.
_LOCAL_STATE_VARIATIONS_LAST_FETCH_TIME_KEY = 'variations_last_fetch_time'
# Test argument to make EG2 test verify the fetch happens in current app launch.
_VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG = '--verify-fetched-in-current-launch'
LOGGER = logging.getLogger(__name__)


class VariationsSimulatorParallelTestRunner(SimulatorParallelTestRunner):
  """Variations simulator runner."""

  def __init__(self, app_path, host_app_path, iossim_path, version, platform,
               out_dir, variations_seed_path, **kwargs):
    super(VariationsSimulatorParallelTestRunner,
          self).__init__(app_path, host_app_path, iossim_path, version,
                         platform, out_dir, **kwargs)
    self.variations_seed_path = variations_seed_path
    self.host_app_bundle_id = test_apps.get_bundle_id(self.host_app_path)
    self.test_app = self.get_launch_test_app()

  def _user_data_dir(self):
    """Returns path to user data dir containing "Local State" file.

    Note: The path is under app data directory of host Chrome app under test.
    The path changes each time launching app but the content is consistent.
    """
    # This is required for next cmd to work.
    iossim_util.boot_simulator_if_not_booted(self.udid)
    app_data_path = iossim_util.get_app_data_directory(self.host_app_bundle_id,
                                                       self.udid)
    return os.path.join(app_data_path, 'Library', 'Application Support',
                        'Google', 'Chrome')

  def _reset_last_fetch_time(self):
    """Resets last fetch time to one day before so the next fetch can happen.

    On mobile devices the fetch will only happen 30 min after last fetch by
    checking |variations_last_fetch_time| key in Local State.
    """
    # Last fetch time in local state uses win timestamp in microseconds.
    win_delta = datetime.utcnow() - datetime(1601, 1, 1)
    win_now = int(win_delta.total_seconds())
    win_one_day_before = win_now - 60 * 60 * 24
    win_one_day_before_microseconds = win_one_day_before * 1000000

    seed_helper.update_local_state(
        self._user_data_dir(), {
            _LOCAL_STATE_VARIATIONS_LAST_FETCH_TIME_KEY:
                str(win_one_day_before_microseconds)
        })
    LOGGER.info('Reset last fetch time to %s in Local State.' %
                win_one_day_before_microseconds)

  def _launch_app_once(self, out_sub_dir, verify_fetched_within_launch=False):
    """Launches app once.

    Args:
      out_sub_dir: (str) Sub dir under |self.out_dir| for this attempt output.
      verify_fetched_within_launch: (bool) Whether to verify that the fetch
        would happens in current launch.

    Returns:
      (test_result_util.ResultCollection): Raw EG test result of the launch.
    """
    launch_out_dir = os.path.join(self.out_dir, out_sub_dir)

    if verify_fetched_within_launch:
      self.test_app.test_args.append(_VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG)

    cmd = self.test_app.command(launch_out_dir, 'id=%s' % self.udid, 1)
    proc = subprocess.Popen(
        cmd,
        env=self.env_vars or {},
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    output = test_runner.print_process_output(proc, self.readline_timeout)

    if _VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG in self.test_app.test_args:
      self.test_app.test_args.remove(_VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG)
    return XcodeLogParser.collect_test_results(launch_out_dir, output)

  def _launch_variations_smoke_test(self):
    """Runs variations smoke test logic which involves multiple test launches.

    Returns:
      Tuple of (bool, str) Success status and reason.
    """
    # Launch app to make it fetch seed from server.
    fetch_launch_result = self._launch_app_once(
        'fetch_launch', verify_fetched_within_launch=True)
    if not fetch_launch_result.passed_tests():
      log = 'Test failure at app launch to fetch variations seed.'
      LOGGER.error(log)
      return False, log

    # Verify a production version of variations seed was fetched successfully.
    current_seed, current_signature = seed_helper.get_current_seed(
        self._user_data_dir())
    if not current_seed or not current_signature:
      log = 'Failed to fetch variations seed on initial fetch launch.'
      LOGGER.error(log)
      return False, log

    # Inject the test seed.
    # |seed_helper.load_test_seed_from_file()| tries to find a seed file under
    # src root first. If it doesn't exist, it will fallback to the one in
    # |self.variations_seed_path|.
    seed, signature = seed_helper.load_test_seed_from_file(
        self.variations_seed_path)
    if not seed or not signature:
      log = ('Ill-formed test seed json file: "%s" and "%s" are required',
             seed_helper.LOCAL_STATE_SEED_NAME,
             seed_helper.LOCAL_STATE_SEED_SIGNATURE_NAME)
      return False, log
    if not seed_helper.inject_test_seed(seed, signature, self._user_data_dir()):
      log = 'Failed to inject test seed.'
      LOGGER.error(log)
      return False, log

    # Launch app with injected seed.
    injected_launch_result = self._launch_app_once('injected_launch')
    if not injected_launch_result.passed_tests():
      log = 'Test failure at app launch after the seed is injected.'
      LOGGER.error(log)
      return False, log

    # Reset last fetch timestamp to one day before now. On mobile devices a
    # fetch will only happen after 30 min of last fetch.
    self._reset_last_fetch_time()

    # Launch app again to refetch and update the injected seed with a delta.
    update_launch_result = self._launch_app_once(
        'update_launch', verify_fetched_within_launch=True)
    if not update_launch_result.passed_tests():
      log = 'Test failure at app launch to update seed with a delta.'
      LOGGER.error(log)
      return False, log

    # Verify seed has been updated successfully and it's different from the
    # injected test seed.
    #
    # TODO(crbug.com/40191854): This test expectation may not work correctly
    # when a field trial config under test does not affect a platform, so it
    # requires more investigations to figure out the correct behavior.
    current_seed, current_signature = seed_helper.get_current_seed(
        self._user_data_dir())
    if current_seed == seed or current_signature == signature:
      log = 'Failed to update seed with a delta'
      LOGGER.error(log)
      return False, log

    return True, 'Variations smoke test passed all steps!'

  def launch(self):
    """Entrance to launch tests in this runner."""
    success, log = self._launch_variations_smoke_test()

    test_status = TestStatus.PASS if success else TestStatus.FAIL
    # Report a single test named |VariationsSmokeTest| as part of runner output.
    overall_result = ResultCollection(test_results=[
        TestResult('VariationsSmokeTest', test_status, test_log=log)
    ])
    overall_result.report_to_result_sink()
    self.test_results = overall_result.standard_json_output(path_delimiter='/')
    self.logs.update(overall_result.test_runner_logs())
    self.tear_down()

    return success
