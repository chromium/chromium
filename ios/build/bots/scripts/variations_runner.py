# Copyright 2021 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runner class for variations smoke tests."""
import json
import logging
import os
import subprocess

import iossim_util
import test_apps
import test_runner
from test_result_util import ResultCollection, TestResult, TestStatus
from xcodebuild_runner import SimulatorParallelTestRunner
import xcode_log_parser

# Constants around the variation keys.
_LOCAL_STATE_SEED_NAME = 'variations_compressed_seed'
_LOCAL_STATE_SEED_SIGNATURE_NAME = 'variations_seed_signature'
_LOCAL_STATE_VARIATIONS_LAST_FETCH_TIME_KEY = 'variations_last_fetch_time'
_ONE_DAY_IN_MICROSECONDS = 24 * 3600 * 1000000
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
    self.log_parser = xcode_log_parser.get_parser()
    self.test_app = self.get_launch_test_app()

  def _local_state_path(self):
    """Returns path to "Local State" file of host app at this time."""
    # This is required for next cmd to work.
    iossim_util.boot_simulator_if_not_booted(self.udid)
    # The path changes each time launching app but the content is consistent.
    app_data_path = iossim_util.get_app_data_directory(self.host_app_bundle_id,
                                                       self.udid)
    return os.path.join(app_data_path, 'Library', 'Application Support',
                        'Google', 'Chrome', 'Local State')

  def _read_local_state(self, local_state_path=None):
    """Reads contents of Local State file."""
    local_state_path = local_state_path or self._local_state_path()
    with open(local_state_path, 'r') as f:
      local_state = json.load(f)
    return local_state

  def _update_local_state(self, update_dict, local_state_path=None):
    """Updates Local State with given |update_dict|."""
    local_state_path = local_state_path or self._local_state_path()
    local_state = self._read_local_state(local_state_path=local_state_path)
    local_state.update(update_dict)
    with open(local_state_path, 'w') as f:
      json.dump(local_state, f)

  def _write_accepted_eula(self):
    """Writes eula accepted to Local State.

    This is needed once. Chrome host app doesn't have it accepted and variations
    seed fetching requires it.
    """
    self._update_local_state({'EulaAccepted': True})
    LOGGER.info('Wrote EulaAccepted: true to Local State.')

  def _reset_last_fetch_time(self):
    """Resets last fetch time to one day before so the next fetch can happen.

    On mobile devices the fetch will only happen 30 min after last fetch by
    checking |variations_last_fetch_time| key in Local State.
    """
    local_state_path = self._local_state_path()
    local_state = self._read_local_state(local_state_path=local_state_path)
    last_fetch_time = int(
        local_state.get('_LOCAL_STATE_VARIATIONS_LAST_FETCH_TIME_KEY',
                        _ONE_DAY_IN_MICROSECONDS))
    one_day_before_last_fetch = str(last_fetch_time - _ONE_DAY_IN_MICROSECONDS)
    self._update_local_state(
        {
            _LOCAL_STATE_VARIATIONS_LAST_FETCH_TIME_KEY:
                one_day_before_last_fetch
        },
        local_state_path=local_state_path)
    LOGGER.info('Reset last fetch time to %s in Local State.' %
                one_day_before_last_fetch)

  def _parse_test_seed(self):
    """Reads and parses the test variations seed.

    For prototypeing propose, a test seed is hard-coded in a given location as
    an argument passed into the test runner. This function should be updated to
    use the seed provided by the official variations test recipe once it's ready
    for integration.

    Returns:
      A tuple of two strings: the compressed seed and the seed signature.
    """
    with open(self.variations_seed_path) as f:
      seed_json = json.load(f)

    return (seed_json.get(_LOCAL_STATE_SEED_NAME,
                          None), seed_json.get(_LOCAL_STATE_SEED_NAME, None))

  def _inject_test_seed(self, seed, signature):
    """Injects given test seed to Local State.

    Args:
      seed (str): A variations seed.
      signature (str): A seed signature.
    """
    seed_dict = {
        _LOCAL_STATE_SEED_NAME: seed,
        _LOCAL_STATE_SEED_SIGNATURE_NAME: signature
    }
    self._update_local_state(seed_dict)
    LOGGER.info('Injected seed and signature to Local State.')

  def _get_current_seed(self):
    """Gets the current seed.

    Returns:
      A tuple of two strings: the compressed seed and the seed signature.
    """
    local_state = self._read_local_state()

    return local_state.get(_LOCAL_STATE_SEED_NAME, None), local_state.get(
        _LOCAL_STATE_SEED_SIGNATURE_NAME, None)

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
    output = test_runner.print_process_output(proc)

    if _VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG in self.test_app.test_args:
      self.test_app.test_args.remove(_VERIFY_FETCHED_IN_CURRENT_LAUNCH_ARG)
    return self.log_parser.collect_test_results(launch_out_dir, output)

  def _launch_variations_smoke_test(self):
    """Runs variations smoke test logic which involves multiple test launches.

    Returns:
      Tuple of (bool, str) Success status and reason.
    """
    # Launch app once to install app and create Local State file.
    first_launch_result = self._launch_app_once('first_launch')
    # Test will fail because there isn't EulaAccepted pref in Local State and no
    # fetch will happen.
    if first_launch_result.passed_tests():
      log = 'Test passed (expected to fail) at first launch (to install app).'
      LOGGER.error(log)
      return False, log

    self._write_accepted_eula()

    # Launch app to make it fetch seed from server.
    fetch_launch_result = self._launch_app_once(
        'fetch_launch', verify_fetched_within_launch=True)
    if not fetch_launch_result.passed_tests():
      log = 'Test failure at app launch to fetch variations seed.'
      LOGGER.error(log)
      return False, log

    # Verify a production version of variations seed was fetched successfully.
    current_seed, current_signature = self._get_current_seed()
    if not current_seed or not current_signature:
      log = 'Failed to fetch variations seed on initial fetch launch.'
      LOGGER.error(log)
      return False, log

    # Inject the test seed.
    seed, signature = self._parse_test_seed()
    if not seed or not signature:
      log = ('Ill-formed test seed json file: "%s" and "%s" are required',
             _LOCAL_STATE_SEED_NAME, _LOCAL_STATE_SEED_SIGNATURE_NAME)
      return False, log
    self._inject_test_seed(seed, signature)

    # Verify the seed has been injected successfully.
    current_seed, current_signature = self._get_current_seed()
    if current_seed != seed or current_signature != signature:
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
    # TODO(crbug.com/1234171): This test expectation may not work correctly when
    # a field trial config under test does not affect a platform, so it requires
    # more investigations to figure out the correct behavior.
    current_seed, current_signature = self._get_current_seed()
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
