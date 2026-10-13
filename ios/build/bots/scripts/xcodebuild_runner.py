# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test runner for running tests using xcodebuild."""

import collections
import glob
import json
import logging
import os
import plistlib
import subprocess
import sys
import time
from typing import Tuple, List, Optional  # noqa: F401
import weakref

import constants
import iossim_util
import test_apps
import test_runner_errors
import shard_util
from test_result_util import ResultCollection, TestResult, TestStatus  # noqa: F401
import test_runner
from xcode_log_parser import XcodeLogParser, Xcode16LogParser
import xcode_util

# if the current directory is in scripts, then we need to add plugin
# path in order to import from that directory
if os.path.split(os.path.dirname(__file__))[1] != 'plugin':
  sys.path.append(
    os.path.join(os.path.abspath(os.path.dirname(__file__)), 'plugin')
  )
from plugin_utils import init_plugins_from_args
from test_plugin_service import TestPluginServicerWrapper, TestPluginServicer

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
CHROMIUM_SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, '../../../..'))
sys.path.append(
  os.path.abspath(os.path.join(CHROMIUM_SRC_DIR, 'build/util/lib/proto'))
)
import measures  # noqa: E402

LOGGER = logging.getLogger(__name__)
MAXIMUM_TESTS_PER_SHARD_FOR_RERUN = 20
XTDEVICE_FOLDER = os.path.expanduser('~/Library/Developer/XCTestDevices')


def _tests_decided_at_runtime(app_name):
  """Return if tests in app are selected at runtime by app_name.

  This works for suites defined in chromium infra.
  """
  suite_name_fragments = ['ios_chrome_multitasking_eg', '_flaky_eg']
  return any(fragment in app_name for fragment in suite_name_fragments)


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

  def __init__(
    self,
    egtests_app,
    udid,
    clones,
    retries,
    readline_timeout,
    exception_checker,
    test_runner,
    out_dir=os.path.basename(os.getcwd()),
    use_clang_coverage=False,
    env=None,
    test_plugin_service=None,
    cert_path=None,
    ensure_fresh_simulators=True,
  ):
    """Initialize launch command.

    Args:
      egtests_app: (EgtestsApp) An egtests_app to run.
      udid: (str) UDID of a device/simulator.
      clones: (int) A number of simulator clones to run test cases against.
      readline_timeout: (int) Timeout to kill a test process when it doesn't
        have output (in seconds).
      exception_checker: (ExceptionChecker) Checks logs for possible infra
        issues and raises them as exceptions.
      test_runner: (SimulatorParallelTestRunner) The test runner this
        LaunchCommand belongs to.
      retries: (int) A number of retries.
      out_dir: (str) A folder in which xcodebuild will generate test output.
        By default it is a current directory.
      env: (dict) Environment variables.
      cert_path: (str) A path for cert to install.
      ensure_fresh_simulators: (bool) Whether to ensure all simulators have a
        fresh state before running tests.

    Raises:
      AppNotFoundError: At incorrect egtests_app parameter type.
    """
    if not isinstance(egtests_app, test_apps.EgtestsApp):
      raise test_runner.AppNotFoundError(
        'Parameter `egtests_app` is not EgtestsApp: %s' % egtests_app
      )
    self.egtests_app = egtests_app
    self.udid = udid
    self.clones = clones
    self.readline_timeout = readline_timeout
    self.retries = retries
    self.out_dir = out_dir
    self.use_clang_coverage = use_clang_coverage
    self.env = env
    self.test_plugin_service = test_plugin_service
    self.cert_path = cert_path
    self.ensure_fresh_simulators = ensure_fresh_simulators
    self.exception_checker = exception_checker
    self.test_runner = test_runner

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
    return test_runner.print_process_output(
      proc,
      timeout=self.readline_timeout,
      exception_checker=self.exception_checker,
    )

  def _save_perf_samples_before_reset(self):
    """Saves the previous attempt's perf samples before the app is wiped.

    Retries reinstall the app (or erase the simulator), which deletes the
    perf_samples.json written by earlier attempts.
    """
    if getattr(self.test_runner, 'benchmark_name', None):
      self.test_runner.save_perf_samples()

  def launch(self):
    """Launches tests using xcodebuild."""
    overall_launch_command_result = ResultCollection()
    clones = self.clones
    running_tests = set(self.egtests_app.get_all_tests())
    attempt_count = measures.count('test_attempts', 'eg')
    # total number of attempts is self.retries+1
    for attempt in range(self.retries + 1):
      attempt_count.record()
      # Cleanup any running plugin process before each attempt
      if self.test_plugin_service:
        self.test_plugin_service.reset()
      # Erase all simulators per each attempt
      if iossim_util.is_device_with_udid_simulator(self.udid):
        if self.ensure_fresh_simulators and attempt > 0:
          self._save_perf_samples_before_reset()
          self.udid = self.test_runner.ensure_fresh_simulator_state()
          # kill all running simulators to prevent possible memory leaks
          test_runner.SimulatorTestRunner.kill_simulators()
        if self.cert_path:
          iossim_util.copy_trusted_certificate(self.cert_path, self.udid)

        with measures.time_consumption(
          'Simulator full boot',
          'XcodeBuildRunner',
          'Pre launch For testing',
          f'Test attempt {attempt}',
        ):
          iossim_util.ensure_simulator_fully_booted(self.udid, num_attempts=2)

        # ideally this should be the last step before running tests, because
        # it boots the simulator.
        iossim_util.disable_simulator_keyboard_tutorial(self.udid)
      elif attempt > 0 and isinstance(self.test_runner, DeviceXcodeTestRunner):
        # Uninstall apps on physical devices before retrying to terminate any
        # orphaned runner or host app processes left behind by a previous hung
        # xcodebuild attempt.
        self._save_perf_samples_before_reset()
        self.test_runner.uninstall_apps()

      outdir_attempt = os.path.join(self.out_dir, 'attempt_%d' % attempt)
      cmd_list = self.egtests_app.command(
        outdir_attempt, 'id=%s' % self.udid, clones
      )

      # TODO(crbug.com/40606422): add heartbeat logging to xcodebuild_runner.
      LOGGER.info(
        'Start test attempt #%d for command [%s]'
        % (attempt, ' '.join(cmd_list))
      )
      output = self.launch_attempt(cmd_list)

      if xcode_util.using_xcode_16_or_higher():
        result = Xcode16LogParser.collect_test_results(outdir_attempt, output)
      else:
        result = XcodeLogParser.collect_test_results(
          outdir_attempt, output, clones > 1
        )

      tests_selected_at_runtime = _tests_decided_at_runtime(
        self.egtests_app.test_app_path
      )
      # For most suites, only keep crash status from last attempt since retries
      # will cover any missing tests. For these decided at runtime, retain
      # crashes from all attempts and a dummy "crashed" result will be reported
      # to indicate some tests might never ran.
      # TODO(crbug.com/40782444): Switch back to excluded tests and set
      # |overall_crash| to always True.
      overall_launch_command_result.add_result_collection(
        result, overwrite_crash=not tests_selected_at_runtime
      )
      result.report_to_result_sink()

      # If result represents a crash and simulator caching is enabled, purge the
      # simulator from the cache to ensure the cached simulator's state is not
      # the source of the crash.
      if result.crashed and iossim_util.is_device_with_udid_simulator(
        self.udid
      ):
        self.test_runner.delete_cached_simulator()

      tests_to_include = set()
      # |running_tests| are compiled tests in target intersecting with swarming
      # sharding. For some suites, they are more than what's needed to run.
      if not tests_selected_at_runtime:
        tests_to_include = tests_to_include | (
          running_tests - overall_launch_command_result.expected_tests()
        )
      # Add failed tests from last rounds for runtime decided suites and device
      # suites.
      tests_to_include = (
        tests_to_include | overall_launch_command_result.never_expected_tests()
      )
      # Do not retry ASan failures
      asan_failures = overall_launch_command_result.asan_failed_tests()
      if asan_failures:
        LOGGER.info('Skipping retrying ASan failures.')
        tests_to_include = tests_to_include - asan_failures
      self.egtests_app.included_tests = list(tests_to_include)

      # Nothing to run in retry.
      if not self.egtests_app.included_tests:
        break

      # If tests are not completed(interrupted or did not start) and there are
      # >= 20 remaining tests, run them with the same number of clones.
      # otherwise re-run with clones=1.
      if (
        not result.crashed
        # If need to re-run less than 20 tests, 1 shard should be enough.
        or (
          len(running_tests)
          - len(overall_launch_command_result.expected_tests())
          <= MAXIMUM_TESTS_PER_SHARD_FOR_RERUN
        )
      ):
        clones = 1

    return overall_launch_command_result


class SimulatorParallelTestRunner(test_runner.SimulatorTestRunner):
  """Class for running simulator tests using xCode."""

  def __init__(
    self,
    app_path,
    host_app_path,
    version,
    platform,
    out_dir,
    **kwargs,
  ):
    """Initializes a new instance of SimulatorParallelTestRunner class.

    Args:
      app_path: (str) A path to egtests_app.
      host_app_path: (str) A path to the host app for EG2.
      version: (str) iOS version to run simulator on.
      platform: (str) Name of device.
      out_dir: (str) A directory to emit test data into.
      (Following are potential args in **kwargs)
      release: (bool) Whether this test runner is running for a release build.
      repeat_count: (int) Number of times to run each test (passed to test app).
      retries: (int) A number to retry test run, will re-run only failed tests.
      clones: (int) A number of clones to run tests against. Default is 1.
      test_cases: (list) List of tests to be included in the test run.
                  None or [] to include all tests.
      test_args: List of strings to pass as arguments to the test when
        launching.
      use_clang_coverage: Whether code coverage is enabled in this run.
      use_simulator_cache: Whether to use prelaunched simulators in the cache
        for this run.
      env_vars: List of environment variables to pass to the test itself.

    Raises:
      AppNotFoundError: If the given app does not exist.
      PlugInsNotFoundError: If the PlugIns directory does not exist for XCTests.
      XcodeVersionNotFoundError: If the given Xcode version does not exist.
      XCTestPlugInNotFoundError: If the .xctest PlugIn does not exist.
    """
    kwargs['retries'] = kwargs.get('retries') or 0
    super(SimulatorParallelTestRunner, self).__init__(
      app_path, platform, version, out_dir, **kwargs
    )
    if self.platform and 'duo' in self.platform.lower() and self.clones > 1:
      LOGGER.info(
        'Forcing clones=1 for %s because parallel simulator clones interfere '
        'with vendor HID posture and orientation events.',
        self.platform,
      )
      self.clones = 1
    self.set_up()
    self.host_app_path = None
    if host_app_path != 'NO_PATH':
      self.host_app_path = os.path.abspath(host_app_path)
    self.logs = collections.OrderedDict()
    self.release = kwargs.get('release') or False
    self.test_results['path_delimiter'] = '/'
    self.use_simulator_cache = kwargs.get('use_simulator_cache') or False

    self.record_video_option = kwargs.get('record_video_option')
    self.skip_enumerate_tests = kwargs.get('skip_enumerate_tests') or False
    self.benchmark_name = kwargs.get('benchmark_name')

    self.all_eg_test_names = []
    if not self._should_skip_enumerate_tests():
      self.all_eg_test_names = self.fetch_test_names()
    self.resolve_eg_test_cases()

    # initializing test plugin service
    # TODO(crbug.com/40933880): remove the legacy code of video recording
    # support as we have migrated to native xcode video recording.
    self.test_plugin_service = None
    enabled_plugins = init_plugins_from_args(
      os.path.join(self.out_dir, self.udid), **kwargs
    )
    if len(enabled_plugins) > 0:
      LOGGER.info(
        'Number of enabled plugins are greater than 0, initiating'
        + 'test plugin service... Enabled plugins are %s' % enabled_plugins
      )
      self.test_plugin_service = TestPluginServicerWrapper(
        TestPluginServicer(enabled_plugins)
      )
    else:
      LOGGER.info('No plugins are enabled, test plugin service will not start.')

  def _should_skip_enumerate_tests(self) -> bool:
    """Returns whether to skip enumerating tests in the EG test bundle."""
    if self.skip_enumerate_tests:
      return True
    if shard_util.gtest_total_shards() <= 1 and self.test_cases:
      return True
    return False

  @property
  def xcode_platform_dir_name(self):
    if self.platform_type == constants.IOSPlatformType.TVOS:
      return 'AppleTVSimulator.platform'
    return 'iPhoneSimulator.platform'

  def _create_xctest_run_enum_tests(self, include_disabled: bool) -> str:
    """Creates xctestrun file used for enumerating tests.

    Returns:
      A path to the generated xctestrun file.
    """
    test_app = self.get_launch_test_app()
    xctestrun_data = test_app.fill_xctestrun_node(include_disabled)

    xctestrun = os.path.join(self.out_dir, 'enumerate_tests.xctestrun')
    with open(xctestrun, 'wb') as f:
      plistlib.dump(xctestrun_data, f)

    return xctestrun

  def fetch_test_names(
    self, include_disabled: bool = False
  ) -> List[Tuple[str, str]]:
    xctestrun = self._create_xctest_run_enum_tests(include_disabled)
    all_test_classes = []
    error_message = ""
    num_attempts = 4

    # Preboot simulator and measure boot time
    if iossim_util.is_device_with_udid_simulator(self.udid):
      with measures.time_consumption(
        'Simulator full boot',
        'XcodeBuildRunner',
        'Pre launch for enumerate test cases',
      ):
        iossim_util.ensure_simulator_fully_booted(self.udid, num_attempts=2)

    for attempt in range(num_attempts):
      # reset error_message with each attempt
      error_message = ""
      enumerate_tests_json = os.path.join(
        os.path.abspath(self.out_dir),
        'enumerate_tests_%d.json' % int(time.time()),
      )

      cmd = [
        "xcodebuild",
        "test-without-building",
        "-enumerate-tests",
        "-xctestrun",
        xctestrun,
        "-destination",
        'id=%s' % self.udid,
        "-test-enumeration-format",
        "json",
        "-test-enumeration-output-path",
        enumerate_tests_json,
        "-collect-test-diagnostics",
        "never",
      ]
      LOGGER.info(cmd)

      start = time.perf_counter()
      proc = subprocess.Popen(
        cmd,
        env=None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
      )
      test_runner.print_process_output(
        proc,
        timeout=self.readline_timeout,
        exception_checker=self.exception_checker,
      )
      end = time.perf_counter()
      elapsed = end - start
      LOGGER.info(
        f'xcodebuild -enumerate-tests (attempt {attempt + 1} of '
        f'{num_attempts}) completed in {elapsed:.2f} seconds'
      )

      # enumerate_tests_json will not be written if return code is non-zero
      if not os.path.isfile(enumerate_tests_json):
        LOGGER.error(
          f'xcodebuild -enumerate-tests failed to create {enumerate_tests_json}'
        )
        continue

      with open(enumerate_tests_json, "r") as f:
        json_output = json.load(f)

      if 'errors' in json_output.keys() and json_output['errors']:
        error_message = '\n'.join(json_output['errors'])
        LOGGER.error(error_message)
      else:
        try:
          all_test_classes = json_output['values'][0]['children'][0]['children']
          if all_test_classes:
            break
        except (KeyError, IndexError, TypeError):
          # retry if keys or list elements are missing
          LOGGER.exception('Failed to parse enumerate-tests json output')
          continue

    # on certain occasions -enumerate-tests will return code 0 and have an empty
    # "errors" list in its json output, but still have failed, in which case
    # all_test_classes will be empty
    if error_message:
      raise test_runner_errors.XcodeEnumerateTestsError(error_message)
    elif not all_test_classes:
      raise test_runner_errors.XcodeEnumerateTestsError()

    all_test_names = []
    for test_class in all_test_classes:
      test_class_name = test_class['name']
      test_methods = test_class.get('children', [])

      for test_method in test_methods:
        all_test_names.append((test_class_name, test_method['name']))

    return all_test_names

  def resolve_eg_test_cases(self):
    gtest_total_shards = shard_util.gtest_total_shards()
    if gtest_total_shards > 1:
      # overwrite assignment to self.test_cases (that already occurred in parent
      # class) if we are running EG tests on multiple swarming shards
      if self.test_cases:
        LOGGER.warning(
          'Overwriting self.test_cases with sharded test cases. '
          'Original test cases: %s' % self.test_cases
        )
      self.test_cases = shard_util.shard_eg_test_cases(self.all_eg_test_names)

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
      self.all_eg_test_names,
      self.platform_type,
      xcode_platform_dir_name=self.xcode_platform_dir_name,
      included_tests=self.test_cases,
      env_vars=self.env_vars,
      test_args=self.test_args,
      release=self.release,
      repeat_count=self.repeat_count,
      host_app_path=self.host_app_path,
      record_video_option=self.record_video_option,
    )

  def launch(self):
    """Launches tests using xcodebuild."""
    if self.test_plugin_service:
      self.test_plugin_service.start_server()
    test_app = self.get_launch_test_app()
    launch_command = LaunchCommand(
      test_app,
      udid=self.udid,
      clones=self.clones,
      readline_timeout=self.readline_timeout,
      retries=self.retries,
      out_dir=os.path.join(self.out_dir, self.udid),
      use_clang_coverage=(
        hasattr(self, 'use_clang_coverage') and self.use_clang_coverage
      ),
      env=self.get_launch_env(),
      test_plugin_service=self.test_plugin_service,
      exception_checker=self.exception_checker,
      test_runner=weakref.proxy(self),  # avoid reference cycle
    )

    try:
      overall_result = launch_command.launch()

      # Adds disabled tests to result. This will output all disabled tests
      # present in the test app binary. Since there's no use in
      # dividing up disabled tests across swarming shards we should only bother
      # outputting them on the first shard
      if self.output_disabled_tests and shard_util.gtest_shard_index() == 0:
        if not self._should_skip_enumerate_tests():
          disabled_tests = self.fetch_test_names(include_disabled=True)
          test_app.disabled_tests = list(
            map(lambda test: f'{test[0]}/{test[1]}', disabled_tests)
          )
          overall_result.add_and_report_test_names_status(
            test_app.disabled_tests,
            TestStatus.SKIP,
            expected_status=TestStatus.SKIP,
            test_log='Test disabled.',
          )

      if self.benchmark_name:
        self._collect_perf_artifacts()

      # Deletes simulator used in the tests after tests end.
      if iossim_util.is_device_with_udid_simulator(self.udid):
        iossim_util.delete_simulator_by_udid(self.udid)

      # Adds unexpectedly skipped tests to result if applicable.
      tests_selected_at_runtime = _tests_decided_at_runtime(self.app_path)
      unexpectedly_skipped = []
      # TODO(crbug.com/40672177): For the multitasking or any flaky test suites,
      # |all_tests_to_run| contains more tests than what actually runs.
      if not tests_selected_at_runtime:
        # |all_tests_to_run| takes into consideration that only a subset of
        # tests may have run due to the test sharding logic in shard_util.py.
        all_tests_to_run = set(launch_command.egtests_app.get_all_tests())
        unexpectedly_skipped = list(
          all_tests_to_run - overall_result.all_test_names()
        )
        overall_result.add_and_report_test_names_status(
          unexpectedly_skipped,
          TestStatus.SKIP,
          test_log=(
            'The test is compiled in test target but was unexpectedly '
            'not run or not finished.'
          ),
        )

      # Add a final crash status to result collection. It will be reported as
      # part of step log in LUCI build.
      if unexpectedly_skipped or overall_result.crashed:
        overall_result.set_crashed_with_prefix(
          crash_message_prefix_line=(
            'Test application crash happened and may result in missing tests:'
          )
        )

      self.test_results = overall_result.standard_json_output(
        path_delimiter='/'
      )
      self.logs.update(overall_result.test_runner_logs())

      if self.benchmark_name:
        root_perf_results = os.path.join(self.out_dir, 'perf_results.json')
        sub_perf_results = os.path.join(self.out_dir, '*', 'perf_results.json')
        has_root_results = os.path.isfile(root_perf_results)
        has_sub_results = any(glob.iglob(sub_perf_results))
        if not (has_root_results or has_sub_results):
          LOGGER.error(
            'Benchmark %s completed without producing perf_results.json.',
            self.benchmark_name,
          )
          return False

      # Return False when:
      # - There are unexpected tests (all results of the tests are unexpected),
      # or
      # - The overall status is crashed and tests are selected at runtime. (i.e.
      # runner is unable to know if all scheduled tests appear in result.)
      return not overall_result.never_expected_tests() and not (
        tests_selected_at_runtime and overall_result.crashed
      )
    finally:
      self.tear_down()

  @staticmethod
  def _convert_samples_to_histogram_sets(
    samples_json_path: str, out_dir: str
  ) -> None:
    """Converts raw sample dicts into Catapult HistogramSet JSON files."""
    repo_root = os.path.abspath(
      os.path.join(os.path.dirname(__file__), '..', '..', '..', '..')
    )
    catapult_tracing = os.path.join(
      repo_root, 'third_party', 'catapult', 'tracing'
    )
    if catapult_tracing not in sys.path:
      sys.path.insert(0, catapult_tracing)
    import tracing_project

    tracing_project.UpdateSysPathIfNeeded()
    from tracing.value import histogram as catapult_histogram
    from tracing.value import histogram_set as catapult_histogram_set
    from tracing.value.diagnostics import generic_set
    from tracing.value.diagnostics import reserved_infos

    with open(samples_json_path, 'r') as f:
      raw_entries = json.load(f)

    grouped_histograms = collections.defaultdict(
      catapult_histogram_set.HistogramSet
    )
    for entry in raw_entries:
      name = entry.get('name')
      unit = entry.get('unit')
      samples = entry.get('samples') or []
      if not name or not unit or not samples:
        continue
      hist = catapult_histogram.Histogram(name, unit)
      for value in samples:
        hist.AddSample(float(value))
      benchmark = entry.get('benchmark') or ''
      # The perf dashboard's JSON converter skips histograms without
      # diagnostics, so always attach the benchmark and story.
      if benchmark:
        hist.diagnostics[reserved_infos.BENCHMARKS.name] = (
          generic_set.GenericSet([benchmark])
        )
      hist.diagnostics[reserved_infos.STORIES.name] = generic_set.GenericSet(
        [entry.get('story') or name]
      )
      grouped_histograms[benchmark].AddHistogram(hist)

    for benchmark, histograms in grouped_histograms.items():
      if benchmark:
        dest_dir = os.path.join(out_dir, benchmark)
        os.makedirs(dest_dir, exist_ok=True)
      else:
        dest_dir = out_dir
      perf_results_path = os.path.join(dest_dir, 'perf_results.json')
      with open(perf_results_path, 'w') as f:
        json.dump(histograms.AsDicts(), f, indent=2)

  @staticmethod
  def _merge_perf_samples(saved_entries, new_entries):
    """Merges perf sample entries, appending samples for matching metrics.

    Entries match on (benchmark, name, unit), the same key PerfAppInterface
    uses when it appends to perf_samples.json.
    """
    merged = [dict(entry) for entry in saved_entries]
    index = {
      (e.get('benchmark') or '', e.get('name'), e.get('unit')): e
      for e in merged
    }
    for entry in new_entries:
      key = (entry.get('benchmark') or '', entry.get('name'), entry.get('unit'))
      if key in index:
        index[key]['samples'] = list(index[key].get('samples') or []) + list(
          entry.get('samples') or []
        )
      else:
        copy = dict(entry)
        merged.append(copy)
        index[key] = copy
    return merged

  def _pull_perf_samples(self):
    """Pulls perf_samples.json from the app container.

    Returns:
      The list of sample entries, or [] if none could be pulled.
    """
    os.makedirs(self.out_dir, exist_ok=True)
    extracted_samples = os.path.join(
      self.out_dir, 'Documents', 'perf_samples.json'
    )
    # Remove any earlier copy so a failed pull isn't mistaken for new data.
    if os.path.isfile(extracted_samples):
      os.remove(extracted_samples)
    target_app_path = self.host_app_path or self.app_path
    if target_app_path and os.path.exists(target_app_path):
      try:
        bundle_id = test_apps.get_bundle_id(target_app_path)
        if bundle_id:
          self.extract_test_data(bundle_id=bundle_id)
      except Exception as err:  # pylint: disable=broad-except
        LOGGER.warning('Perf artifact collection failed: %s', err)
    if not os.path.isfile(extracted_samples):
      return []
    try:
      with open(extracted_samples, 'r') as f:
        return json.load(f)
    except ValueError as err:
      LOGGER.warning('Failed to parse %s: %s', extracted_samples, err)
      return []

  def save_perf_samples(self):
    """Saves perf samples from the current app install before it's wiped."""
    self._saved_perf_samples = self._merge_perf_samples(
      getattr(self, '_saved_perf_samples', []), self._pull_perf_samples()
    )

  def _collect_perf_artifacts(self):
    """Pulls perf_samples.json and converts it to HistogramSet JSON."""
    self.save_perf_samples()
    entries = self._saved_perf_samples
    if not entries:
      return
    raw_samples_dest = os.path.join(self.out_dir, 'perf_samples.json')
    with open(raw_samples_dest, 'w') as f:
      json.dump(entries, f, indent=2)
    try:
      self._convert_samples_to_histogram_sets(raw_samples_dest, self.out_dir)
    except Exception as err:  # pylint: disable=broad-except
      LOGGER.warning('Perf artifact conversion failed: %s', err)

  def ensure_fresh_simulator_state(self) -> str:
    """Ensures that the simulator is in a fresh state.

    Returns:
      The UDID of the fresh simulator. In the case of cache being used, a new
        fresh clone with a new UDID may be created. If caching is not enabled,
        the existing simulator will be wiped back to a fresh state returning its
        UDID.
    """
    # Clean xcode managed simulator clones
    iossim_util.shutdown_all_simulators(XTDEVICE_FOLDER)
    iossim_util.delete_all_simulators(XTDEVICE_FOLDER)

    if self.use_simulator_cache:
      # delete current simulator
      iossim_util.delete_simulator_by_udid(self.udid)
      # create new simulator & update UDID
      self.udid = iossim_util.get_simulator(
        self.platform, self.version, self.out_dir, self.use_simulator_cache
      )
      return self.udid

    # wipe existing simulator
    iossim_util.wipe_simulator_by_udid(self.udid)
    return self.udid

  def tear_down(self):
    if self.test_plugin_service:
      LOGGER.info('Shutting down test plugin service')
      self.test_plugin_service.tear_down()


class DeviceXcodeTestRunner(
  SimulatorParallelTestRunner, test_runner.DeviceTestRunner
):
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

    # SimulatorParallelTestRunner.get_launch_test_app() needs
    # self.platform_type, which is set in its constructor, but it is not
    # called by this constructor. Since device tests are only supported on iOS
    # at the moment, just hardcode its value here.
    self.platform_type = constants.IOSPlatformType.IPHONEOS

    self.clones = 1  # For tests on real devices clones=1
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
    self.skip_enumerate_tests = kwargs.get('skip_enumerate_tests') or False
    self.benchmark_name = kwargs.get('benchmark_name')
    self.test_plugin_service = None
    self.all_eg_test_names = []
    if not self._should_skip_enumerate_tests():
      self.all_eg_test_names = self.fetch_test_names()
    self.resolve_eg_test_cases()

  @property
  def xcode_platform_dir_name(self):
    return 'iPhoneOS.platform'

  def set_up(self):
    """Performs setup actions which must occur prior to every test launch."""
    self.uninstall_apps()
    self.wipe_derived_data()
    self.restart_usbmuxd()

  def extract_test_data(self, bundle_id: str | None = None):
    """Extracts data emitted by the test."""
    test_runner.DeviceTestRunner.extract_test_data(self, bundle_id=bundle_id)

  def tear_down(self):
    """Performs cleanup actions which must occur after every test launch."""
    test_runner.DeviceTestRunner.tear_down(self)
