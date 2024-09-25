#!/usr/bin/env vpython3
# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Runs telemetry benchmarks and gtest perf tests.

If optional argument --isolated-script-test-output=[FILENAME] is passed
to the script, json is written to that file in the format detailed in
//docs/testing/json-test-results-format.md.

If optional argument --isolated-script-test-filter=[TEST_NAMES] is passed to
the script, it should be a  double-colon-separated ("::") list of test names,
to run just that subset of tests.

This script is intended to be the base command invoked by the isolate,
followed by a subsequent Python script. It could be generalized to
invoke an arbitrary executable.
It currently runs several benchmarks. The benchmarks it will execute are
based on the shard it is running on and the sharding_map_path.

If this is executed with a gtest perf test, the flag --non-telemetry
has to be passed in to the script so the script knows it is running
an executable and not the run_benchmark command.

This script merges test results from all the benchmarks into the one
output.json file. The test results and perf results are also put in separate
directories per benchmark. Two files will be present in each directory;
perf_results.json, which is the perf specific results (with unenforced format,
could be histogram or graph json), and test_results.json.

TESTING:
To test changes to this script, please run unit tests:
$ cd testing/scripts
$ python3 -m unittest run_performance_tests_unittest.py

Run end-to-end tests:
$ cd tools/perf
$ ./run_tests ScriptsSmokeTest.testRunPerformanceTests
"""

import argparse
from collections import OrderedDict
import json
import os
import pathlib
import shutil
import sys
import time
import tempfile
import traceback

import six

import requests

import common

CHROMIUM_SRC_DIR = pathlib.Path(__file__).absolute().parents[2]
RELEASE_DIR = CHROMIUM_SRC_DIR / 'out/Release'

PERF_DIR = CHROMIUM_SRC_DIR / 'tools/perf'
sys.path.append(str(PERF_DIR))
# //tools/perf imports.
if (PERF_DIR / 'crossbench_result_converter.py').exists():
  # Optional import needed to run crossbench.
  import crossbench_result_converter
else:
  print('Optional crossbench_result_converter not available.')
import generate_legacy_perf_dashboard_json
from core import path_util

PERF_CORE_DIR = PERF_DIR / 'core'
sys.path.append(str(PERF_CORE_DIR))
# //tools/perf/core imports.
import results_merger

sys.path.append(str(CHROMIUM_SRC_DIR / 'testing'))
# //testing imports.
import xvfb
import test_env

THIRD_PARTY_DIR = CHROMIUM_SRC_DIR / 'third_party'
CATAPULT_DIR = THIRD_PARTY_DIR / 'catapult'
TELEMETRY_DIR = CATAPULT_DIR / 'telemetry'
# //third_party/catapult/telemetry imports.
if TELEMETRY_DIR.exists() and (CATAPULT_DIR / 'common').exists():
  # Telemetry is required on perf infra, but not present on some environments.
  sys.path.append(str(TELEMETRY_DIR))
  from telemetry.internal.browser import browser_finder
  from telemetry.internal.browser import browser_options
  from telemetry.internal.util import binary_manager
else:
  print('Optional telemetry library not available.')

SHARD_MAPS_DIR = CHROMIUM_SRC_DIR / 'tools/perf/core/shard_maps'
CROSSBENCH_TOOL = CHROMIUM_SRC_DIR / 'third_party/crossbench/cb.py'
ADB_TOOL = THIRD_PARTY_DIR / 'android_sdk/public/platform-tools/adb'
PAGE_SETS_DATA = CHROMIUM_SRC_DIR / 'tools/perf/page_sets/data'
PERF_TOOLS = ['benchmarks', 'executables', 'crossbench']

# See https://crbug.com/923564.
# We want to switch over to using histograms for everything, but converting from
# the format output by gtest perf tests to histograms has introduced several
# problems. So, only perform the conversion on tests that are whitelisted and
# are okay with potentially encountering issues.
GTEST_CONVERSION_WHITELIST = [
    'angle_perftests',
    'base_perftests',
    'blink_heap_perftests',
    'blink_platform_perftests',
    'cc_perftests',
    'components_perftests',
    'command_buffer_perftests',
    'dawn_perf_tests',
    'gpu_perftests',
    'load_library_perf_tests',
    'net_perftests',
    'browser_tests',
    'services_perftests',
    # TODO(jmadill): Remove once migrated. http://anglebug.com/5124
    'standalone_angle_perftests',
    'sync_performance_tests',
    'tracing_perftests',
    'views_perftests',
    'viz_perftests',
    'wayland_client_perftests',
    'xr.vr.common_perftests',
]

# pylint: disable=useless-object-inheritance


class OutputFilePaths(object):
  """Provide paths to where results outputs should be written.

  The process_perf_results.py merge script later will pull all of these
  together, so that's why they aren't in the standard locations. Also,
  note that because of the OBBS (One Build Bot Step), Telemetry
  has multiple tests running on a single shard, so we need to prefix
  these locations with a directory named by the benchmark name.
  """

  def __init__(self, isolated_out_dir, perf_test_name):
    self.name = perf_test_name
    self.benchmark_path = os.path.join(isolated_out_dir, perf_test_name)

  def SetUp(self):
    if os.path.exists(self.benchmark_path):
      shutil.rmtree(self.benchmark_path)
    os.makedirs(self.benchmark_path)
    return self

  @property
  def perf_results(self):
    return os.path.join(self.benchmark_path, 'perf_results.json')

  @property
  def test_results(self):
    return os.path.join(self.benchmark_path, 'test_results.json')

  @property
  def logs(self):
    return os.path.join(self.benchmark_path, 'benchmark_log.txt')

  @property
  def csv_perf_results(self):
    """Path for csv perf results.

    Note that the chrome.perf waterfall uses the json histogram perf results
    exclusively. csv_perf_results are implemented here in case a user script
    passes --output-format=csv.
    """
    return os.path.join(self.benchmark_path, 'perf_results.csv')


def print_duration(step, start):
  print('Duration of %s: %d seconds' % (step, time.time() - start))


def IsWindows():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


class GtestCommandGenerator(object):

  def __init__(self,
               options,
               override_executable=None,
               additional_flags=None,
               ignore_shard_env_vars=False):
    self._options = options
    self._override_executable = override_executable
    self._additional_flags = additional_flags or []
    self._ignore_shard_env_vars = ignore_shard_env_vars

  def generate(self, output_dir):
    """Generate the command to run to start the gtest perf test.

    Returns:
      list of strings, the executable and its arguments.
    """
    return ([self._get_executable()] + self._generate_filter_args() +
            self._generate_repeat_args() +
            self._generate_also_run_disabled_tests_args() +
            self._generate_output_args(output_dir) +
            self._generate_shard_args() + self._get_additional_flags())

  @property
  def ignore_shard_env_vars(self):
    return self._ignore_shard_env_vars

  @property
  def executable_name(self):
    """Gets the platform-independent name of the executable."""
    return self._override_executable or self._options.executable

  def _get_executable(self):
    executable = str(self.executable_name)
    if IsWindows():
      return r'.\%s.exe' % executable
    return './%s' % executable

  def _get_additional_flags(self):
    return self._additional_flags

  def _generate_shard_args(self):
    """Teach the gtest to ignore the environment variables.

    GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS will confuse the gtest
    and convince it to only run some of its tests. Instead run all
    of them.
    """
    if self._ignore_shard_env_vars:
      return ['--test-launcher-total-shards=1', '--test-launcher-shard-index=0']
    return []

  def _generate_filter_args(self):
    if self._options.isolated_script_test_filter:
      filter_list = common.extract_filter_list(
          self._options.isolated_script_test_filter)
      return ['--gtest_filter=' + ':'.join(filter_list)]
    return []

  def _generate_repeat_args(self):
    # TODO(crbug.com/40608634): Support --isolated-script-test-repeat.
    return []

  def _generate_also_run_disabled_tests_args(self):
    # TODO(crbug.com/40608634): Support
    # --isolated-script-test-also-run-disabled-tests.
    return []

  def _generate_output_args(self, output_dir):
    output_args = []
    if self._options.use_gtest_benchmark_script:
      output_args.append('--output-dir=' + output_dir)
    # These flags are to make sure that test output perf metrics in the log.
    if '--verbose' not in self._get_additional_flags():
      output_args.append('--verbose')
    if ('--test-launcher-print-test-stdio=always'
        not in self._get_additional_flags()):
      output_args.append('--test-launcher-print-test-stdio=always')
    return output_args


def write_simple_test_results(return_code, output_filepath, benchmark_name):
  # TODO(crbug.com/40144432): Fix to output
  # https://chromium.googlesource.com/chromium/src/+/main/docs/testing/json_test_results_format.md
  # for each test rather than this summary.
  # Append the shard index to the end of the name so that the merge script
  # doesn't blow up trying to merge unmergeable results.
  benchmark_name += '_shard_%s' % os.environ.get('GTEST_SHARD_INDEX', '0')
  output_json = {
      'tests': {
          benchmark_name: {
              'expected': 'PASS',
              'actual': 'FAIL' if return_code else 'PASS',
              'is_unexpected': bool(return_code),
          },
      },
      'interrupted': False,
      'path_delimiter': '/',
      'version': 3,
      'seconds_since_epoch': time.time(),
      'num_failures_by_type': {
          'FAIL': 1 if return_code else 0,
          'PASS': 0 if return_code else 1,
      },
  }
  with open(output_filepath, 'w') as fh:
    json.dump(output_json, fh)


def upload_simple_test_results(return_code, benchmark_name):
  # TODO(crbug.com/40144432): Fix to upload results for each test rather than
  # this summary.
  try:
    with open(os.environ['LUCI_CONTEXT']) as f:
      sink = json.load(f)['result_sink']
  except KeyError:
    return

  if return_code:
    summary = '<p>Benchmark failed with status code %d</p>' % return_code
  else:
    summary = '<p>Benchmark passed</p>'

  result_json = {
      'testResults': [{
          'testId': benchmark_name,
          'expected': not return_code,
          'status': 'FAIL' if return_code else 'PASS',
          'summaryHtml': summary,
          'tags': [{
              'key': 'exit_code',
              'value': str(return_code)
          }],
      }]
  }

  res = requests.post(
      url='http://%s/prpc/luci.resultsink.v1.Sink/ReportTestResults' %
      sink['address'],
      headers={
          'Content-Type': 'application/json',
          'Accept': 'application/json',
          'Authorization': 'ResultSink %s' % sink['auth_token'],
      },
      data=json.dumps(result_json))
  res.raise_for_status()


def execute_gtest_perf_test(command_generator,
                            output_paths,
                            use_xvfb=False,
                            is_unittest=False,
                            results_label=None):
  start = time.time()

  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'
  # TODO(crbug.com/40153230): Some gtests do not implements the
  # unit_test_launcher.cc. As a result, they will not respect the arguments
  # added by _generate_shard_args() and will still use the values of
  # GTEST_SHARD_INDEX and GTEST_TOTAL_SHARDS to run part of the tests.
  # Removing those environment variables as a workaround.
  if command_generator.ignore_shard_env_vars:
    if 'GTEST_TOTAL_SHARDS' in env:
      env.pop('GTEST_TOTAL_SHARDS')
    if 'GTEST_SHARD_INDEX' in env:
      env.pop('GTEST_SHARD_INDEX')

  return_code = 0
  try:
    command = command_generator.generate(output_paths.benchmark_path)
    if use_xvfb:
      # When running with xvfb, we currently output both to stdout and to the
      # file. It would be better to only output to the file to keep the logs
      # clean.
      return_code = xvfb.run_executable(command,
                                        env,
                                        stdoutfile=output_paths.logs)
    else:
      with open(output_paths.logs, 'w') as handle:
        try:
          return_code = test_env.run_command_output_to_handle(command,
                                                              handle,
                                                              env=env)
        except OSError as e:
          print('Command to run gtest perf test %s failed with an OSError: %s' %
                (output_paths.name, e))
          return_code = 1
    if (not os.path.exists(output_paths.perf_results)
        and os.path.exists(output_paths.logs)):
      # Get the correct json format from the stdout to write to the perf
      # results file if gtest does not generate one.
      results_processor = generate_legacy_perf_dashboard_json.\
          LegacyResultsProcessor()
      graph_json_string = results_processor.GenerateJsonResults(
          output_paths.logs)
      with open(output_paths.perf_results, 'w') as fh:
        fh.write(graph_json_string)
  except Exception:  # pylint: disable=broad-except
    traceback.print_exc()
    return_code = 1
  if os.path.exists(output_paths.perf_results):
    executable_name = command_generator.executable_name
    if executable_name.startswith('bin/run_'):
      # The executable is a wrapper used by Fuchsia. Remove the prefix to get
      # the actual executable name.
      executable_name = executable_name[8:]
    if executable_name in GTEST_CONVERSION_WHITELIST:
      with path_util.SysPath(path_util.GetTracingDir()):
        # pylint: disable=no-name-in-module,import-outside-toplevel
        from tracing.value import gtest_json_converter
        # pylint: enable=no-name-in-module,import-outside-toplevel
      gtest_json_converter.ConvertGtestJsonFile(output_paths.perf_results,
                                                label=results_label)
  else:
    print('ERROR: gtest perf test %s did not generate perf output' %
          output_paths.name)
    return_code = 1
  write_simple_test_results(return_code, output_paths.test_results,
                            output_paths.name)
  if not is_unittest:
    upload_simple_test_results(return_code, output_paths.name)

  print_duration('executing gtest %s' % command_generator.executable_name,
                 start)

  return return_code


class _TelemetryFilterArgument(object):

  def __init__(self, filter_string):
    self.benchmark, self.story = filter_string.split('/')


class TelemetryCommandGenerator(object):

  def __init__(self,
               benchmark,
               options,
               story_selection_config=None,
               is_reference=False):
    self.benchmark = benchmark
    self._options = options
    self._story_selection_config = story_selection_config
    self._is_reference = is_reference

  def generate(self, output_dir):
    """Generate the command to run to start the benchmark.

    Args:
      output_dir: The directory to configure the command to put output files
        into.

    Returns:
      list of strings, the executable and its arguments.
    """
    return (
        [sys.executable] + self._options.executable.split(' ') +
        [self.benchmark] + self._generate_filter_args() +
        self._generate_also_run_disabled_tests_args() +
        self._generate_output_args(output_dir) +
        self._generate_story_selection_args() +
        # passthrough args must be before reference args and repeat args:
        # crbug.com/928928, crbug.com/894254#c78
        self._get_passthrough_args() + self._generate_syslog_args() +
        self._generate_repeat_args() + self._generate_reference_build_args() +
        self._generate_results_label_args())

  def _get_passthrough_args(self):
    return self._options.passthrough_args

  def _generate_filter_args(self):
    if self._options.isolated_script_test_filter:
      filter_list = common.extract_filter_list(
          self._options.isolated_script_test_filter)
      filter_arguments = [_TelemetryFilterArgument(f) for f in filter_list]
      applicable_stories = [
          f.story for f in filter_arguments if f.benchmark == self.benchmark
      ]
      # Need to convert this to a valid regex.
      filter_regex = '(' + '|'.join(applicable_stories) + ')'
      return ['--story-filter=' + filter_regex]
    return []

  def _generate_repeat_args(self):
    pageset_repeat = None
    if self._options.isolated_script_test_repeat:
      pageset_repeat = self._options.isolated_script_test_repeat
    elif (self._story_selection_config is not None
          and self._story_selection_config.get('pageset_repeat')):
      pageset_repeat = self._story_selection_config.get('pageset_repeat')

    if pageset_repeat:
      return ['--pageset-repeat=' + str(pageset_repeat)]
    return []

  def _generate_also_run_disabled_tests_args(self):
    if self._options.isolated_script_test_also_run_disabled_tests:
      return ['--also-run-disabled-tests']
    return []

  def _generate_output_args(self, output_dir):
    if self._options.no_output_conversion:
      return ['--output-format=none', '--output-dir=' + output_dir]
    return [
        '--output-format=json-test-results', '--output-format=histograms',
        '--output-dir=' + output_dir
    ]

  def _generate_story_selection_args(self):
    """Returns arguments that limit the stories to be run inside the benchmark.
    """
    selection_args = []
    if self._story_selection_config:
      if 'begin' in self._story_selection_config:
        selection_args.append('--story-shard-begin-index=%d' %
                              (self._story_selection_config['begin']))
      if 'end' in self._story_selection_config:
        selection_args.append('--story-shard-end-index=%d' %
                              (self._story_selection_config['end']))
      if 'sections' in self._story_selection_config:
        range_string = self._generate_story_index_ranges(
            self._story_selection_config['sections'])
        if range_string:
          selection_args.append('--story-shard-indexes=%s' % range_string)
      if self._story_selection_config.get('abridged', True):
        selection_args.append('--run-abridged-story-set')
    return selection_args

  def _generate_syslog_args(self):
    if self._options.per_test_logs_dir:
      isolated_out_dir = os.path.dirname(
          self._options.isolated_script_test_output)
      return ['--logs-dir', os.path.join(isolated_out_dir, self.benchmark)]
    return []

  def _generate_story_index_ranges(self, sections):
    range_string = ''
    for section in sections:
      begin = section.get('begin', '')
      end = section.get('end', '')
      # If there only one story in the range, we only keep its index.
      # In general, we expect either begin or end, or both.
      if begin != '' and end != '' and end - begin == 1:
        new_range = str(begin)
      elif begin != '' or end != '':
        new_range = '%s-%s' % (str(begin), str(end))
      else:
        raise ValueError('Index ranges in "sections" in shard map should have'
                         'at least one of "begin" and "end": %s' % str(section))
      if range_string:
        range_string += ',%s' % new_range
      else:
        range_string = new_range
    return range_string

  def _generate_reference_build_args(self):
    if self._is_reference:
      reference_browser_flag = '--browser=reference'
      # TODO(crbug.com/40113070): Make the logic generic once more reference
      # settings are added
      if '--browser=android-chrome-bundle' in self._get_passthrough_args():
        reference_browser_flag = '--browser=reference-android-chrome-bundle'
      return [reference_browser_flag, '--max-failures=5']
    return []

  def _generate_results_label_args(self):
    if self._options.results_label:
      return ['--results-label=' + self._options.results_label]
    return []


def execute_telemetry_benchmark(command_generator,
                                output_paths,
                                use_xvfb=False,
                                return_exit_code_zero=False,
                                no_output_conversion=False):
  start = time.time()

  env = os.environ.copy()
  env['CHROME_HEADLESS'] = '1'

  return_code = 1
  temp_dir = tempfile.mkdtemp('telemetry')
  infra_failure = False
  try:
    command = command_generator.generate(temp_dir)
    if use_xvfb:
      # When running with xvfb, we currently output both to stdout and to the
      # file. It would be better to only output to the file to keep the logs
      # clean.
      return_code = xvfb.run_executable(command,
                                        env=env,
                                        stdoutfile=output_paths.logs)
    else:
      with open(output_paths.logs, 'w') as handle:
        return_code = test_env.run_command_output_to_handle(command,
                                                            handle,
                                                            env=env)
    expected_results_filename = os.path.join(temp_dir, 'test-results.json')
    if os.path.exists(expected_results_filename):
      shutil.move(expected_results_filename, output_paths.test_results)
    else:
      common.write_interrupted_test_results_to(output_paths.test_results, start)

    if not no_output_conversion:
      expected_perf_filename = os.path.join(temp_dir, 'histograms.json')
      if os.path.exists(expected_perf_filename):
        shutil.move(expected_perf_filename, output_paths.perf_results)
      elif return_code:
        print(f'The benchmark failed with status code {return_code}, '
              'and did not produce perf results output. '
              'Check benchmark output for more details.')
      else:
        print('The benchmark returned a success status code, '
              'but did not product perf results output.')

    csv_file_path = os.path.join(temp_dir, 'results.csv')
    if os.path.isfile(csv_file_path):
      shutil.move(csv_file_path, output_paths.csv_perf_results)
  except Exception:  # pylint: disable=broad-except
    print('The following exception may have prevented the code from '
          'outputing structured test results and perf results output:')
    print(traceback.format_exc())
    infra_failure = True
  finally:
    # On swarming bots, don't remove output directory, since Result Sink might
    # still be uploading files to Result DB. Also, swarming bots automatically
    # clean up at the end of each task.
    if 'SWARMING_TASK_ID' not in os.environ:
      # Add ignore_errors=True because otherwise rmtree may fail due to leaky
      # processes of tests are still holding opened handles to files under
      # |tempfile_dir|. For example, see crbug.com/865896
      shutil.rmtree(temp_dir, ignore_errors=True)

  print_duration('executing benchmark %s' % command_generator.benchmark, start)

  if infra_failure:
    print('There was an infrastructure error encountered during the run. '
          'Please check the logs above for details')
    return 1

  # Telemetry sets exit code to -1 to indicate that no stories were run. This
  # becomes 255 on linux because linux doesn't support -1 so it does modulo:
  # -1 % 256 == 255.
  # TODO(crbug.com/40105219): Make 111 be the exit code that means
  # "no stories were run.".
  if return_code in (111, -1, 255):
    print('Exit code %s indicates that no stories were run, so we are marking '
          'this as a success.' % return_code)
    return 0
  if return_code:
    if return_exit_code_zero:
      print('run_benchmark returned exit code ' + str(return_code) +
            ' which indicates there were test failures in the run.')
      return 0
    return return_code
  return 0


def load_map_file(map_file, isolated_out_dir):
  """Loads the shard map file and copies it to isolated_out_dir."""
  if not os.path.exists(map_file):
    map_file_path = SHARD_MAPS_DIR / map_file
    if map_file_path.exists():
      map_file = str(map_file_path)
    else:
      raise Exception(f'Test shard map file not found: {map_file_path}')
  copy_map_file_to_out_dir(map_file, isolated_out_dir)
  with open(map_file) as f:
    return json.load(f)


def load_map_string(map_string, isolated_out_dir):
  """Loads the dynamic shard map string and writes it to isolated_out_dir."""
  if not map_string:
    raise Exception('Use `--dynamic-shardmap` to pass the dynamic shard map')
  shard_map = json.loads(map_string, object_pairs_hook=OrderedDict)
  with tempfile.NamedTemporaryFile(mode='wb', delete=False) as tmp:
    tmp.write(bytes(map_string, 'utf-8'))
    tmp.close()
    copy_map_file_to_out_dir(tmp.name, isolated_out_dir)
  return shard_map


def copy_map_file_to_out_dir(map_file, isolated_out_dir):
  """Copies the sharding map file to isolated_out_dir for later collection."""
  if not os.path.exists(isolated_out_dir):
    os.makedirs(isolated_out_dir)
  shutil.copyfile(map_file,
                  os.path.join(isolated_out_dir, 'benchmarks_shard_map.json'))


def fetch_binary_path(dependency_name, os_name='linux', arch='x86_64'):
  if binary_manager.NeedsInit():
    binary_manager.InitDependencyManager(None)
  return binary_manager.FetchPath(dependency_name, os_name=os_name, arch=arch)


class CrossbenchTest(object):
  """This class is for running Crossbench tests.

  To run Crossbench tests, pass the relative path to `cb.py` as the
  `executable` argument, followed by `--benchmarks` to specify the
  target benchmark. The remaining Crossbench arguments are optional,
  and any passed arguments are sent to the `cb.py` for executing the test.

  Shard map: Use `crossbench-benchmarks` node in each shard group with a
  dictionary of benchmark names and, optionally, a list of arguments that need
  to pass through the `cb.py` tool. See `linux-perf-fyi_map.json` for examples.

  Example:
    ./run_performance_tests.py ../../third_party/crossbench/cb.py \
    --isolated-script-test-output=/tmp/crossbench/ \
    --benchmarks=speedometer \
    --browser=../../out/linux/chrome \
    --repeat=1 --probe='profiling' --story='jQuery.*'
  """

  EXECUTABLE = 'cb.py'
  OUTDIR = '--out-dir=%s/output'
  CHROME_BROWSER = '--browser=%s'
  ANDROID_HJSON = '{browser:"%s", driver:{type:"Android", adb_bin:"%s"}}'
  STORY_LABEL = 'default'
  BENCHMARK_FILESERVERS = {'speedometer_3.0': 'third_party/speedometer/v3.0'}

  def __init__(self, options, isolated_out_dir):
    self.options = options
    self.isolated_out_dir = isolated_out_dir
    browser_arg = self._get_browser_arg(options.passthrough_args)
    self.is_android = self._is_android(browser_arg)
    self._find_browser(browser_arg)
    self.driver_path_arg = self._find_chromedriver(browser_arg)
    self.network = self._get_network_arg(options.passthrough_args,
                                         self.is_android)

  def _get_browser_arg(self, args):
    browser_arg = self._get_arg(args, '--browser=', must_exists=True)
    return browser_arg.split('=', 1)[1]

  def _get_network_arg(self, args, is_android):
    if _arg := self._get_arg(args, '--network='):
      return [_arg]
    if _arg := self._get_arg(args, '--fileserver'):
      return self._create_fileserver_network(_arg)
    if is_android or self._get_arg(args, '--wpr'):
      return self._create_wpr_network(args)
    return []

  def _create_fileserver_network(self, arg):
    if '=' in arg:
      fileserver_path = arg.split('=', 1)[1]
    else:
      benchmark = self.options.benchmarks
      if benchmark not in self.BENCHMARK_FILESERVERS:
        raise ValueError(f'fileserver does not support {benchmark}')
      fileserver_path = self.BENCHMARK_FILESERVERS.get(benchmark)
    fileserver_relative_path = str(CHROMIUM_SRC_DIR / fileserver_path)
    # Replacing --fileserver with --network.
    self.options.passthrough_args.remove(arg)
    return [
        self._create_network_json('local',
                                  path=fileserver_relative_path,
                                  url='http://localhost:0')
    ]

  def _create_wpr_network(self, args):
    wpr_arg = self._get_arg(args, '--wpr')
    if wpr_arg and '=' in wpr_arg:
      wpr_name = wpr_arg.split('=', 1)[1]
    else:
      # TODO: Use update_wpr library when it supports Crossbench archive files.
      wpr_name = 'crossbench_android_speedometer_3.0_000.wprgo'
    archive = str(PAGE_SETS_DATA / wpr_name)
    if (wpr_go := fetch_binary_path('wpr_go')) is None:
      raise ValueError(f'wpr_go not found: {wpr_go}')
    if wpr_arg:
      # Replacing --wpr with --network.
      self.options.passthrough_args.remove(wpr_arg)
    return [self._create_network_json('wpr', path=archive, wpr_go_bin=wpr_go)]

  def _create_network_json(self, config_type, path, url=None, wpr_go_bin=None):
    network_dict = {'type': config_type}
    network_dict['path'] = path
    if url:
      network_dict['url'] = url
    if wpr_go_bin:
      network_dict['wpr_go_bin'] = wpr_go_bin
    network_json = json.dumps(network_dict)
    return f'--network={network_json}'

  def _get_arg(self, args, arg, must_exists=False):
    if _args := [a for a in args if a.startswith(arg)]:
      if len(_args) != 1:
        raise ValueError(f'Expects exactly one {arg} on command line')
      return _args[0]
    if must_exists:
      raise ValueError(f'{arg} argument is missing!')
    return []

  def _is_android(self, browser_arg):
    """Is the test running on an Android device.

    See third_party/catapult/telemetry/telemetry/internal/backends/android_browser_backend_settings.py  # pylint: disable=line-too-long
    """
    return browser_arg.lower().startswith('android')

  def _find_browser(self, browser_arg):
    # Replacing --browser with the generated self.browser.
    self.options.passthrough_args = [
        arg for arg in self.options.passthrough_args
        if not arg.startswith('--browser=')
    ]
    if '/' in browser_arg or '\\' in browser_arg:
      # The --browser arg looks like a path. Use it as-is.
      self.browser = self.CHROME_BROWSER % browser_arg
      return
    options = browser_options.BrowserFinderOptions()
    options.chrome_root = CHROMIUM_SRC_DIR
    parser = options.CreateParser()
    parser.parse_args([self.CHROME_BROWSER % browser_arg])
    possible_browser = browser_finder.FindBrowser(options)
    if not possible_browser:
      raise ValueError(f'Unable to find Chrome browser of type: {browser_arg}')
    if self.is_android:
      browser_app = possible_browser.settings.package
      android_json = self.ANDROID_HJSON % (browser_app, ADB_TOOL)
      self.browser = self.CHROME_BROWSER % android_json
    else:
      assert hasattr(possible_browser, 'local_executable')
      self.browser = self.CHROME_BROWSER % possible_browser.local_executable

  def _find_chromedriver(self, browser_arg):
    browser_arg = browser_arg.lower()
    if browser_arg == 'release_x64':
      path = '../Release_x64'
    elif self.is_android:
      path = 'clang_x64'
    else:
      path = '.'

    abspath = pathlib.Path(path).absolute()
    if ((driver_path := (abspath / 'chromedriver')).exists()
        or (driver_path := (abspath / 'chromedriver.exe')).exists()):
      return [f'--driver-path={driver_path}']
    # Unable to find ChromeDriver, will rely on crossbench to download one.
    return []

  def _get_default_args(self):
    default_args = [
        '--no-symlinks',
        # Required until crbug/41491492 and crbug/346323630 are fixed.
        '--enable-features=DisablePrivacySandboxPrompts',
    ]
    if not self.is_android:
      # See http://shortn/_xGSaVM9P5g
      default_args.append('--enable-field-trial-config')
    return default_args

  def _generate_command_list(self, benchmark, benchmark_args, working_dir):
    return (['vpython3'] + [self.options.executable] + [benchmark] +
            ['--env-validation=throw'] + [self.OUTDIR % working_dir] +
            [self.browser] + benchmark_args + self.driver_path_arg +
            self.network + self._get_default_args())

  def execute_benchmark(self,
                        benchmark,
                        display_name,
                        benchmark_args,
                        is_unittest=False):
    start = time.time()

    env = os.environ.copy()
    env['CHROME_HEADLESS'] = '1'

    return_code = 1
    output_paths = OutputFilePaths(self.isolated_out_dir, display_name).SetUp()
    infra_failure = False
    try:
      command = self._generate_command_list(benchmark, benchmark_args,
                                            output_paths.benchmark_path)
      if self.options.xvfb:
        # When running with xvfb, we currently output both to stdout and to the
        # file. It would be better to only output to the file to keep the logs
        # clean.
        return_code = xvfb.run_executable(command,
                                          env=env,
                                          stdoutfile=output_paths.logs)
      else:
        with open(output_paths.logs, 'w') as handle:
          return_code = test_env.run_command_output_to_handle(command,
                                                              handle,
                                                              env=env)

      if return_code == 0:
        crossbench_result_converter.convert(
            pathlib.Path(output_paths.benchmark_path) / 'output',
            pathlib.Path(output_paths.perf_results), display_name,
            self.STORY_LABEL, self.options.results_label)
    except Exception:  # pylint: disable=broad-except
      print('The following exception may have prevented the code from '
            'outputing structured test results and perf results output:')
      print(traceback.format_exc())
      infra_failure = True

    write_simple_test_results(return_code, output_paths.test_results,
                              display_name)
    if not is_unittest:
      upload_simple_test_results(return_code, display_name)

    print_duration(f'Executing benchmark: {benchmark}', start)

    if infra_failure:
      print('There was an infrastructure error encountered during the run. '
            'Please check the logs above for details')
      return 1

    if return_code and self.options.ignore_benchmark_exit_code:
      print(f'crossbench returned exit code {return_code}'
            ' which indicates there were test failures in the run.')
      return 0
    return return_code

  def execute(self):
    if not self.options.benchmarks:
      raise Exception('Please use the --benchmarks to specify the benchmark.')
    if ',' in self.options.benchmarks:
      raise Exception('No support to run multiple benchmarks at this time.')
    return self.execute_benchmark(
        self.options.benchmarks,
        (self.options.benchmark_display_name or self.options.benchmarks),
        self.options.passthrough_args)


def parse_arguments(args):
  parser = argparse.ArgumentParser()
  parser.add_argument('executable', help='The name of the executable to run.')
  parser.add_argument('--isolated-script-test-output', required=True)
  # The following two flags may be passed in sometimes by Pinpoint
  # or by the recipe, but they don't do anything. crbug.com/927482.
  parser.add_argument('--isolated-script-test-chartjson-output', required=False)
  parser.add_argument('--isolated-script-test-perf-output', required=False)

  parser.add_argument('--isolated-script-test-filter', type=str, required=False)

  # Note that the following three arguments are only supported by Telemetry
  # tests right now. See crbug.com/920002.
  parser.add_argument('--isolated-script-test-repeat', type=int, required=False)
  parser.add_argument(
      '--isolated-script-test-launcher-retry-limit',
      type=int,
      required=False,
      choices=[0])  # Telemetry does not support retries. crbug.com/894254#c21
  parser.add_argument('--isolated-script-test-also-run-disabled-tests',
                      default=False,
                      action='store_true',
                      required=False)
  parser.add_argument('--xvfb', help='Start xvfb.', action='store_true')
  parser.add_argument('--non-telemetry',
                      help='Type of perf test',
                      type=bool,
                      default=False)
  parser.add_argument('--gtest-benchmark-name',
                      help='Name of the gtest benchmark',
                      type=str,
                      required=False)
  parser.add_argument('--use-gtest-benchmark-script',
                      help='Whether gtest is invoked via benchmark script.',
                      default=False,
                      action='store_true')

  parser.add_argument('--benchmarks',
                      help='Comma separated list of benchmark names'
                      ' to run in lieu of indexing into our benchmark bot maps',
                      required=False)
  parser.add_argument('--benchmark-display-name',
                      help='Benchmark name displayed to the user,'
                      ' supported with crossbench only',
                      required=False)
  # Added to address android flakiness.
  parser.add_argument('--benchmark-max-runs',
                      help='Max number of benchmark runs until it succeeds.',
                      type=int,
                      required=False,
                      default=1)
  # crbug.com/1236245: This allows for per-benchmark device logs.
  parser.add_argument('--per-test-logs-dir',
                      help='Require --logs-dir args for test',
                      required=False,
                      default=False,
                      action='store_true')
  # Some executions may have a different sharding scheme and/or set of tests.
  # These files must live in src/tools/perf/core/shard_maps
  parser.add_argument('--test-shard-map-filename', type=str, required=False)
  parser.add_argument('--run-ref-build',
                      help='Run test on reference browser',
                      action='store_true')
  parser.add_argument('--passthrough-arg',
                      help='Arguments to pass directly through to the test '
                      'executable.',
                      action='append',
                      dest='passthrough_args',
                      default=[])
  parser.add_argument('--use-dynamic-shards',
                      help='If set, use dynamic shardmap instead of the file.',
                      action='store_true',
                      required=False)
  parser.add_argument('--dynamic-shardmap',
                      help='The dynamically generated shardmap string used to '
                      'replace the static shardmap file.',
                      type=str,
                      required=False)
  parser.add_argument('--ignore-benchmark-exit-code',
                      help='If set, return an exit code 0 even if there' +
                      ' are benchmark failures',
                      action='store_true',
                      required=False)
  parser.add_argument('--results-label',
                      help='If set for a non-telemetry test, adds label to' +
                      ' the result histograms.',
                      type=str,
                      required=False)
  parser.add_argument('--no-output-conversion',
                      help='If supplied, trace conversion is not done.',
                      action='store_true',
                      required=False,
                      default=False)
  options, leftover_args = parser.parse_known_args(args)
  options.passthrough_args.extend(leftover_args)
  return options


def main(sys_args):
  args = sys_args[1:]  # Skip program name.
  options = parse_arguments(args)
  isolated_out_dir = os.path.dirname(options.isolated_script_test_output)
  overall_return_code = 0
  # This is a list of test results files to be merged into a standard
  # output.json file for use by infrastructure including FindIt.
  # This list should not contain reference build runs
  # since we do not monitor those. Also, merging test reference build results
  # with standard build results may not work properly.
  test_results_files = []

  print('Running a series of performance test subprocesses. Logs, performance\n'
        'results, and test results JSON will be saved in a subfolder of the\n'
        'isolated output directory. Inside the hash marks in the following\n'
        'lines is the name of the subfolder to find results in.\n')

  if options.use_dynamic_shards:
    shard_map = load_map_string(options.dynamic_shardmap, isolated_out_dir)
    overall_return_code = _run_benchmarks_on_shardmap(shard_map, options,
                                                      isolated_out_dir,
                                                      test_results_files)
  elif options.test_shard_map_filename:
    shard_map = load_map_file(options.test_shard_map_filename, isolated_out_dir)
    overall_return_code = _run_benchmarks_on_shardmap(shard_map, options,
                                                      isolated_out_dir,
                                                      test_results_files)
  elif options.executable.endswith(CrossbenchTest.EXECUTABLE):
    assert options.benchmark_max_runs == 1, (
        'Benchmark rerun is not supported with CrossbenchTest.')
    overall_return_code = CrossbenchTest(options, isolated_out_dir).execute()
  elif options.non_telemetry:
    assert options.benchmark_max_runs == 1, (
        'Benchmark rerun is not supported in non telemetry tests.')
    benchmark_name = options.gtest_benchmark_name
    passthrough_args = options.passthrough_args
    # crbug/1146949#c15
    # In the case that pinpoint passes all arguments to swarming through http
    # request, the passthrough_args are converted into a comma-separated string.
    if passthrough_args and isinstance(passthrough_args, six.text_type):
      passthrough_args = passthrough_args.split(',')
    # With --non-telemetry, the gtest executable file path will be passed in as
    # options.executable, which is different from running on shard map. Thus,
    # we don't override executable as we do in running on shard map.
    command_generator = GtestCommandGenerator(options,
                                              additional_flags=passthrough_args,
                                              ignore_shard_env_vars=True)
    # Fallback to use the name of the executable if flag isn't set.
    # TODO(crbug.com/40588014): remove fallback logic and raise parser error if
    # --non-telemetry is set but --gtest-benchmark-name is not set once pinpoint
    # is converted to always pass --gtest-benchmark-name flag.
    if not benchmark_name:
      benchmark_name = options.executable
    output_paths = OutputFilePaths(isolated_out_dir, benchmark_name).SetUp()
    print('\n### {folder} ###'.format(folder=benchmark_name))
    overall_return_code = execute_gtest_perf_test(
        command_generator,
        output_paths,
        options.xvfb,
        results_label=options.results_label)
    test_results_files.append(output_paths.test_results)
  elif options.benchmarks:
    benchmarks = options.benchmarks.split(',')
    for benchmark in benchmarks:
      command_generator = TelemetryCommandGenerator(benchmark, options)
      for run_num in range(options.benchmark_max_runs):
        print('\n### {folder} (attempt #{num}) ###'.format(folder=benchmark,
                                                           num=run_num))
        output_paths = OutputFilePaths(isolated_out_dir, benchmark).SetUp()
        return_code = execute_telemetry_benchmark(
            command_generator,
            output_paths,
            options.xvfb,
            options.ignore_benchmark_exit_code,
            no_output_conversion=options.no_output_conversion)
        if return_code == 0:
          break
      overall_return_code = return_code or overall_return_code
      test_results_files.append(output_paths.test_results)
    if options.run_ref_build:
      print('Not running reference build. --run-ref-build argument is only '
            'supported for sharded benchmarks. It is simple to support '
            'this for unsharded --benchmarks if needed.')
  else:
    raise Exception('Telemetry tests must provide either a shard map or a '
                    '--benchmarks list so that we know which stories to run.')

  # Dumping the test results.
  if test_results_files:
    test_results_list = []
    for test_results_file in test_results_files:
      if os.path.exists(test_results_file):
        with open(test_results_file, 'r') as fh:
          test_results_list.append(json.load(fh))
    merged_test_results = results_merger.merge_test_results(test_results_list)
    with open(options.isolated_script_test_output, 'w') as f:
      json.dump(merged_test_results, f)

  return overall_return_code


def _run_benchmarks_on_shardmap(shard_map, options, isolated_out_dir,
                                test_results_files):
  overall_return_code = 0
  # TODO(crbug.com/40631538): shard environment variables are not specified
  # for single-shard shard runs.
  if 'GTEST_SHARD_INDEX' not in os.environ and '1' in shard_map.keys():
    raise Exception(
        'Setting GTEST_SHARD_INDEX environment variable is required '
        'when you use a shard map.')
  shard_index = os.environ.get('GTEST_SHARD_INDEX', '0')
  shard_configuration = shard_map[shard_index]
  if not [x for x in shard_configuration if x in PERF_TOOLS]:
    raise Exception(
        f'None of {",".join(PERF_TOOLS)} presented in the shard map')
  if 'benchmarks' in shard_configuration:
    benchmarks_and_configs = shard_configuration['benchmarks']
    for (benchmark, story_selection_config) in benchmarks_and_configs.items():
      # Need to run the benchmark on both latest browser and reference
      # build.
      command_generator = TelemetryCommandGenerator(
          benchmark, options, story_selection_config=story_selection_config)
      for run_num in range(options.benchmark_max_runs):
        output_paths = OutputFilePaths(isolated_out_dir, benchmark).SetUp()
        print('\n### {folder} (attempt #{num}) ###'.format(folder=benchmark,
                                                           num=run_num))
        return_code = execute_telemetry_benchmark(
            command_generator,
            output_paths,
            options.xvfb,
            options.ignore_benchmark_exit_code,
            no_output_conversion=options.no_output_conversion)
        if return_code == 0:
          break
      overall_return_code = return_code or overall_return_code
      test_results_files.append(output_paths.test_results)
      if options.run_ref_build:
        reference_benchmark_foldername = benchmark + '.reference'
        reference_output_paths = OutputFilePaths(
            isolated_out_dir, reference_benchmark_foldername).SetUp()
        reference_command_generator = TelemetryCommandGenerator(
            benchmark,
            options,
            story_selection_config=story_selection_config,
            is_reference=True)
        print(
            '\n### {folder} ###'.format(folder=reference_benchmark_foldername))
        # We intentionally ignore the return code and test results of the
        # reference build.
        execute_telemetry_benchmark(
            reference_command_generator,
            reference_output_paths,
            options.xvfb,
            options.ignore_benchmark_exit_code,
            no_output_conversion=options.no_output_conversion)
  if 'executables' in shard_configuration:
    names_and_configs = shard_configuration['executables']
    for (name, configuration) in names_and_configs.items():
      additional_flags = []
      if 'arguments' in configuration:
        additional_flags = configuration['arguments']
      command_generator = GtestCommandGenerator(
          options,
          override_executable=configuration['path'],
          additional_flags=additional_flags,
          ignore_shard_env_vars=True)
      for run_num in range(options.benchmark_max_runs):
        output_paths = OutputFilePaths(isolated_out_dir, name).SetUp()
        print('\n### {folder} (attempt #{num}) ###'.format(folder=name,
                                                           num=run_num))
        return_code = execute_gtest_perf_test(command_generator, output_paths,
                                              options.xvfb)
        if return_code == 0:
          break
      overall_return_code = return_code or overall_return_code
      test_results_files.append(output_paths.test_results)
  if 'crossbench' in shard_configuration:
    benchmarks = shard_configuration['crossbench']
    # Overwriting the "run_benchmark" with the Crossbench tool.
    options.executable = str(CROSSBENCH_TOOL)
    original_passthrough_args = options.passthrough_args.copy()
    for benchmark, benchmark_config in benchmarks.items():
      display_name = benchmark_config.get('display_name', benchmark)
      if benchmark_args := benchmark_config.get('arguments', []):
        options.passthrough_args.extend(benchmark_args)
      options.benchmarks = benchmark
      crossbench_test = CrossbenchTest(options, isolated_out_dir)
      for run_num in range(options.benchmark_max_runs):
        print(f'\n### {display_name} (attempt #{run_num}) ###')
        return_code = crossbench_test.execute_benchmark(benchmark, display_name,
                                                        [])
        if return_code == 0:
          break
      overall_return_code = return_code or overall_return_code
      test_results_files.append(
          OutputFilePaths(isolated_out_dir, display_name).test_results)
      options.passthrough_args = original_passthrough_args.copy()

  return overall_return_code


# This is not really a "script test" so does not need to manually add
# any additional compile targets.
def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  # Conform minimally to the protocol defined by ScriptTest.
  if 'compile_targets' in sys.argv:
    funcs = {
        'run': None,
        'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))

  sys.exit(main(sys.argv))
