#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import contextlib
import json
import logging
import os
import posixpath
import re
import shutil
import subprocess
import sys
import tempfile
import time

from collections import OrderedDict
from PIL import Image

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
PAR_DIR = os.path.join(SRC_DIR, 'testing')
OUT_DIR = os.path.join(SRC_DIR, 'out', 'Release')
BLINK_DIR = os.path.join(SRC_DIR, 'third_party', 'blink')
BLINK_TOOLS = os.path.join(BLINK_DIR, 'tools')
BLINK_WEB_TESTS = os.path.join(BLINK_DIR, 'web_tests')
BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')

# Protocall buffer directories to import
PYPROTO_LIB = os.path.join(OUT_DIR, 'pyproto', 'google')
WEBVIEW_VARIATIONS_PROTO = os.path.join(OUT_DIR, 'pyproto',
                                        'android_webview', 'proto')

if PYUTILS not in sys.path:
  sys.path.append(PYUTILS)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

if BLINK_TOOLS not in sys.path:
  sys.path.append(BLINK_TOOLS)

if PYPROTO_LIB not in sys.path:
  sys.path.append(PYPROTO_LIB)

if WEBVIEW_VARIATIONS_PROTO not in sys.path:
  sys.path.append(WEBVIEW_VARIATIONS_PROTO)

sys.path.append(PAR_DIR)

if 'compile_targets' not in sys.argv:
  import aw_variations_seed_pb2

import devil_chromium

from blinkpy.common.host import Host
from blinkpy.common.path_finder import PathFinder
from blinkpy.web_tests.models import test_failures
from blinkpy.web_tests.port.android import (
    ANDROID_WEBVIEW, CHROME_ANDROID)
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor

from devil import devil_env
from devil.android import apk_helper
from devil.android import device_temp_file
from devil.android import device_utils
from devil.android import flag_changer
from devil.android import logcat_monitor
from devil.android.tools import script_common
from devil.android.tools import system_app
from devil.android.tools import webview_app
from devil.utils import logging_common
from pylib.local.device import local_device_environment
from pylib.local.emulator import avd
from py_utils.tempfile_ext import NamedTemporaryDirectory
from scripts import common
from skia_gold_common.skia_gold_properties import SkiaGoldProperties
from skia_gold_common import skia_gold_session_manager
from skia_gold_infra import finch_skia_gold_utils

ANDROID_WEBLAYER = 'android_weblayer'
LOGCAT_TAG = 'finch_test_runner_py'
LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
  'WebView*:v',
  '%s:I' % LOGCAT_TAG
]
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
TEST_CASES = {}

def _is_version_greater_than_or_equal(version1, version2):
  version1_parts = version1.split('.')
  version2_parts = version2.split('.')
  for i in range(4):
    comp = int(version1_parts[i]) - int(version2_parts[i])
    if  comp != 0:
      return comp > 0

  return True


def _merge_results_dicts(dict_to_merge, test_results_dict):
  if 'actual' in dict_to_merge:
    test_results_dict.update(dict_to_merge)
    return
  for key in dict_to_merge.keys():
    _merge_results_dicts(dict_to_merge[key],
                             test_results_dict.setdefault(key, {}))


# pylint: disable=super-with-arguments, abstract-method
class FinchTestCase(common.BaseIsolatedScriptArgsAdapter):

  def __init__(self, device):
    self.host = Host()
    self.fs = self.host.filesystem
    self.path_finder = PathFinder(self.fs)
    self.port = self.host.port_factory.get()
    super(FinchTestCase, self).__init__()
    self._add_extra_arguments()
    self._parser = self._override_options(self._parser)
    self._include_filename = None
    self.layout_test_results_subdir = 'layout-test-results'
    self._device = device
    self.parse_args()
    self.port.set_option_default('target', self.options.target)
    self._browser_apk_helper = apk_helper.ToHelper(self.options.browser_apk)

    self.browser_package_name = self._browser_apk_helper.GetPackageName()
    self.browser_activity_name = (self.options.browser_activity_name or
                                  self.default_browser_activity_name)
    self.layout_test_results_subdir = None
    self.test_specific_browser_args = []
    if self.options.webview_provider_apk:
      self.webview_provider_package_name = (
          apk_helper.GetPackageName(self.options.webview_provider_apk))

    # Initialize the Skia Gold session manager
    self._skia_gold_corpus = 'finch-smoke-tests'
    self._skia_gold_tmp_dir = None
    self._skia_gold_session_manager = None

  @classmethod
  def app_user_sub_dir(cls):
    """Returns sub directory within user directory"""
    return 'app_%s' % cls.product_name()

  @classmethod
  def product_name(cls):
    raise NotImplementedError

  @property
  def tests(self):
    return [
      'dom/collections/HTMLCollection-delete.html',
      'dom/collections/HTMLCollection-supported-property-names.html',
      'dom/collections/HTMLCollection-supported-property-indices.html',
    ]

  @property
  def pixel_tests(self):
    return []

  @property
  def default_browser_activity_name(self):
    raise NotImplementedError

  @property
  def default_finch_seed_path(self):
    raise NotImplementedError

  @classmethod
  def finch_seed_download_args(cls):
    return []

  def generate_test_output_args(self, output):
        return ['--log-chromium=%s' % output]

  def generate_test_filter_args(self, test_filter_str):
        included_tests, excluded_tests = \
            self._resolve_tests_from_isolate_filter(test_filter_str)
        include_file, self._include_filename = self.fs.open_text_tempfile()
        with include_file:
            for test in included_tests:
                include_file.write(test)
                include_file.write('\n')
        wpt_args = ['--include-file=%s' % self._include_filename]
        for test in excluded_tests:
            wpt_args.append('--exclude=%s' % test)
        return wpt_args

  def _override_options(self, base_parser):
        """Create a parser that overrides existing options.

        `argument.ArgumentParser` can extend other parsers and override their
        options, with the caveat that the child parser only inherits options
        that the parent had at the time of the child's initialization.

        See Also:
            https://docs.python.org/3/library/argparse.html#parents
        """
        parser = argparse.ArgumentParser(
            parents=[base_parser],
            # Allow overriding existing options in the parent parser.
            conflict_handler='resolve',
            epilog=('All unrecognized arguments are passed through '
                    "to wptrunner. Use '--wpt-help' to see wptrunner's usage."),
        )
        parser.add_argument(
            '--isolated-script-test-repeat',
            '--repeat',
            '--gtest_repeat',
            metavar='REPEAT',
            type=int,
            default=1,
            help='Number of times to run the tests')
        parser.add_argument(
            '--isolated-script-test-launcher-retry-limit',
            '--test-launcher-retry-limit',
            '--retry-unexpected',
            metavar='RETRIES',
            type=int,
            help=(
                'Maximum number of times to rerun unexpectedly failed tests. '
                'Defaults to 3 unless given an explicit list of tests to run.'))
        # `--gtest_filter` and `--isolated-script-test-filter` have slightly
        # different formats and behavior, so keep them as separate options.
        # See: crbug/1316164#c4

        # TODO(crbug.com/1356318): This is a temporary hack to hide the
        # inherited '--xvfb' option and force Xvfb to run always.
        parser.add_argument('--xvfb', action='store_true', default=True,
                            help=argparse.SUPPRESS)
        return parser

  def generate_test_repeat_args(self, repeat_count):
    return ['--repeat=%d' % repeat_count]

  def generate_test_launcher_retry_limit_args(self, retry_limit):
    return ['--retry-unexpected=%d' % retry_limit]

  def generate_sharding_args(self, total_shards, shard_index):
    return ['--total-chunks=%d' % total_shards,
            # shard_index is 0-based but WPT's this-chunk to be 1-based
            '--this-chunk=%d' % (shard_index + 1),
            # The default sharding strategy is to shard by directory. But
            # we want to hash each test to determine which shard runs it.
            # This allows running individual directories that have few
            # tests across many shards.
            '--chunk-type=hash']

  def clean_up_after_test_run(self):
    if self._include_filename:
        self.fs.remove(self._include_filename)

  def new_seed_downloaded(self):
    # TODO(crbug.com/1285152): Implement seed download test
    # for Chrome and WebLayer.
    return True

  def enable_internet(self):
    self._device.RunShellCommand(
        ['settings', 'put', 'global', 'airplane_mode_on', '0'])
    self._device.RunShellCommand(
        ['am', 'broadcast', '-a',
         'android.intent.action.AIRPLANE_MODE'])
    self._device.RunShellCommand(['svc', 'wifi', 'enable'])
    self._device.RunShellCommand(['svc', 'data', 'enable'])

  def disable_internet(self):
    self._device.RunShellCommand(
        ['settings', 'put', 'global', 'airplane_mode_on', '1'])
    self._device.RunShellCommand(
        ['am', 'broadcast', '-a',
         'android.intent.action.AIRPLANE_MODE'])

  @contextlib.contextmanager
  def _archive_logcat(self, filename, endpoint_name):
    start_point = 'START {}'.format(endpoint_name)
    end_point = 'END {}'.format(endpoint_name)
    with logcat_monitor.LogcatMonitor(
        self._device.adb,
        filter_specs=LOGCAT_FILTERS,
        output_file=filename,
        check_error=False):
      try:
        self._device.RunShellCommand(['log', '-p', 'i', '-t', LOGCAT_TAG,
                                      start_point],
                                     check_return=True)
        yield
      finally:
        self._device.RunShellCommand(['log', '-p', 'i', '-t', LOGCAT_TAG,
                                      end_point],
                                     check_return=True)

  def parse_args(self, args=None):
    super(FinchTestCase, self).parse_args(args)
    if (not self.options.finch_seed_path or
        not os.path.exists(self.options.finch_seed_path)):
      logger.warning('Could not find the finch seed passed '
                     'as the argument for --finch-seed-path. '
                     'Running tests on the default finch seed')
      self.options.finch_seed_path = self.default_finch_seed_path

  @property
  def output_directory(self):
      return self.path_finder.path_from_chromium_base('out',
                                                      self.options.target)

  @property
  def mojo_js_directory(self):
      return self.fs.join(self.output_directory, 'gen')

  @property
  def wpt_output(self):
      return self.options.isolated_script_test_output

  @property
  def _raw_log_path(self):
    return self.fs.join(self.output_directory, 'finch-smoke-raw-events.log')

  def __enter__(self):
    self._device.EnableRoot()
    # Run below commands to ensure that the device can download a seed
    self.disable_internet()
    self._device.adb.Emu(['power', 'ac', 'on'])
    self._skia_gold_tmp_dir = tempfile.mkdtemp()
    self._skia_gold_session_manager = (
        skia_gold_session_manager.SkiaGoldSessionManager(
            self._skia_gold_tmp_dir, SkiaGoldProperties(self.options)))
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self._skia_gold_session_manager = None
    if self._skia_gold_tmp_dir:
      shutil.rmtree(self._skia_gold_tmp_dir)
      self._skia_gold_tmp_dir = None

  @property
  def rest_args(self):
    unknown_args = super(FinchTestCase, self).rest_args

    rest_args = list()

    rest_args.extend(self.wpt_rest_args(unknown_args))

    rest_args.extend([
      '--webdriver-arg=--disable-build-check',
      '--device-serial',
      self._device.serial,
      '--webdriver-binary',
      os.path.join('clang_x64', 'chromedriver'),
      '--symbols-path',
      self.output_directory,
      '--package-name',
      self.browser_package_name,
      '--keep-app-data-directory',
      '--test-type=testharness',
    ])

    for binary_arg in self.browser_command_line_args():
      rest_args.append('--binary-arg=%s' % binary_arg)

    for test in self.tests:
      rest_args.extend(['--include', test])

    return rest_args

  @property
  def wpt_binary(self):
    default_wpt_binary = os.path.join(
        common.SRC_DIR, "third_party", "wpt_tools", "wpt", "wpt")
    return os.environ.get("WPT_BINARY", default_wpt_binary)

  @property
  def wpt_root_dir(self):
      return self.path_finder.path_from_web_tests(
          self.path_finder.wpt_prefix())

  @property
  def _wpt_run_args(self):
      """The start of a 'wpt run' command."""
      return [
          self.wpt_binary,
          # Use virtualenv packages installed by vpython, not wpt.
          '--venv=%s' % self.path_finder.chromium_base(),
          '--skip-venv-setup',
          'run',
      ]

  def process_and_upload_results(self, test_name_prefix):
    artifacts_dir=os.path.join(os.path.dirname(self.wpt_output),
                               self.layout_test_results_subdir)
    if self.fs.exists(artifacts_dir):
        self.fs.rmtree(artifacts_dir)
    self.fs.maybe_make_directory(artifacts_dir)
    logger.info('Recreated artifacts directory (%s)', artifacts_dir)

    processor = WPTResultsProcessor(
        self.host.filesystem,
        self.port,
        artifacts_dir=artifacts_dir,
        test_name_prefix=test_name_prefix)

    processor.copy_results_viewer()

    with self.fs.open_text_file_for_reading(self._raw_log_path) as raw_logs:
        for event in map(json.loads, raw_logs):
            if event.get('action') != 'shutdown':
                processor.process_event(event)
    processor.process_results_json(self.wpt_output)

  def wpt_rest_args(self, unknown_args):
    rest_args = list(self._wpt_run_args)
    rest_args.extend([
        '--no-pause-after-test',
        '--no-capture-stdio',
        '--no-manifest-download',
        '--tests=%s' % self.wpt_root_dir,
        '--metadata=%s' % self.wpt_root_dir,
        '--mojojs-path=%s' % self.mojo_js_directory,
        '--log-raw=%s' % self._raw_log_path,
    ])

    if self.options.default_exclude:
        rest_args.extend(['--default-exclude'])

    if self.options.verbose >= 3:
        rest_args.extend([
            '--log-mach=-',
            '--log-mach-level=debug',
            '--log-mach-verbose',
        ])
    if self.options.verbose >= 4:
        rest_args.extend([
            '--webdriver-arg=--verbose',
            '--webdriver-arg="--log-path=-"',
        ])

    rest_args.append(self.wpt_product_name())
    # We pass through unknown args as late as possible so that they can
    # override earlier options. It also allows users to pass test names as
    # positional args, which must not have option strings between them.
    for unknown_arg in unknown_args:
        # crbug/1274933#c14: Some developers had used the end-of-options
        # marker '--' to pass through arguments to wptrunner.
        # crrev.com/c/3573284 makes this no longer necessary.
        if unknown_arg == '--':
            logger.warning(
                'Unrecognized options will automatically fall through '
                'to wptrunner.')
            logger.warning(
                "There is no need to use the end-of-options marker '--'.")
        else:
            rest_args.append(unknown_arg)
    return rest_args

  @classmethod
  def add_common_arguments(cls, parser):
    parser.add_argument('--test-case',
                        choices=TEST_CASES.keys(),
                        # TODO(rmhasan): Remove default values after
                        # adding arguments to test suites. Also make
                        # this argument required.
                        default='webview',
                        help='Name of test case')
    parser.add_argument('--finch-seed-path',
                        type=os.path.realpath,
                        help='Path to the finch seed')
    parser.add_argument('--browser-apk',
                        '--webview-shell-apk',
                        '--weblayer-shell-apk',
                        help='Path to the browser apk',
                        type=os.path.realpath,
                        required=True)
    parser.add_argument('--webview-provider-apk',
                        type=os.path.realpath,
                        help='Path to the WebView provider apk')
    parser.add_argument('--additional-apk',
                        action='append',
                        type=os.path.realpath,
                        default=[],
                        help='List of additional apk\'s to install')
    parser.add_argument('--browser-activity-name',
                        action='store',
                        help='Browser activity name')
    parser.add_argument('--use-webview-installer-tool',
                        action='store_true',
                        help='Use the WebView installer tool.')
    parser.add_argument('--fake-variations-channel',
                        action='store',
                        default='stable',
                        choices=['dev', 'canary', 'beta', 'stable'],
                        help='Finch seed release channel')
    parser.add_argument('-j',
                        '--processes',
                        type=lambda processes: max(0, int(processes)),
                        default=1,
                        help='Number of emulator to run.')
    common.add_emulator_args(parser)
    # Add arguments used by Skia Gold.
    SkiaGoldProperties.AddCommandLineArguments(parser)

  def _add_extra_arguments(self):
    parser = self._parser
    parser.add_argument(
      '-t',
      '--target',
      default='Release',
      help='Target build subdirectory under //out')
    parser.add_argument(
      '--default-exclude',
      action='store_true',
      help=('Only run the tests explicitly given in arguments '
            '(can run no tests, which will exit with code 0)'))
    parser.add_argument(
      '-v',
      '--verbose',
      action='count',
      default=0,
      help='Increase verbosity')
    self.add_product_specific_argument_groups(parser)
    self.add_common_arguments(parser)

  @classmethod
  def add_product_specific_argument_groups(cls, _):
    pass

  def _compare_screenshots_with_baselines(self, all_pixel_tests_results_dict):
    """Compare pixel tests screenshots with baselines stored in skia gold

    Args:
      all_pixel_tests_results_dict: Results dictionary for all pixel tests

    Returns:
      1 if there was an error comparing images otherwise 0
    """
    skia_gold_session = (
        self._skia_gold_session_manager.GetSkiaGoldSession(
            {'platform': 'android'}, self._skia_gold_corpus))

    def _process_test_leaf(test_result_dict):
      if ('artifacts' not in test_result_dict or
          'actual_image' not in test_result_dict['artifacts']):
        return 0

      return_code = 0
      artifacts_dict = test_result_dict['artifacts']
      curr_artifacts = list(artifacts_dict.keys())
      for artifact_name in curr_artifacts:
        artifact_path = artifacts_dict[artifact_name][0]
        # Compare screenshots to baselines stored in Skia Gold
        status, error = skia_gold_session.RunComparison(
            artifact_path,
            os.path.join(os.path.dirname(self.wpt_output), artifact_path))

        if status:
          test_result_dict['actual'] = 'FAIL'
          all_pixel_tests_results_dict['num_failures_by_type'].setdefault(
              'FAIL', 0)
          all_pixel_tests_results_dict['num_failures_by_type']['FAIL'] += 1
          triage_link = finch_skia_gold_utils.log_skia_gold_status_code(
              skia_gold_session, artifact_path, status, error)
          if triage_link:
            artifacts_dict['%s_triage_link' % artifact_name] = [triage_link]
          return_code = 1
        else:
          test_result_dict['actual'] = 'PASS'

      return return_code

    def _process_tests(node):
      return_code = 0
      if 'actual' in node:
        return _process_test_leaf(node)
      for next_node in node.values():
        return_code |= _process_tests(next_node)
      return return_code

    return _process_tests(all_pixel_tests_results_dict['tests'])

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    self._device.Uninstall(self.browser_package_name)
    self._device.Install(self.options.browser_apk, reinstall=True)
    for apk_path in self.options.additional_apk:
      self._device.Install(apk_path)

    self._device.ClearApplicationState(
        self.browser_package_name,
        permissions=self._browser_apk_helper.GetPermissions())

    # TODO(rmhasan): For R+ test devices, store the files in the
    # app's data directory. This is needed for R+ devices because
    # of the scoped storage feature.
    tests_root_dir = posixpath.join(self._device.GetExternalStoragePath(),
                                    'chromium_tests_root')
    local_device_environment.place_nomedia_on_device(self._device,
                                                     tests_root_dir)

    # Store screenshot tests on the device's external storage.
    for test_file in self.pixel_tests:
      self._device.RunShellCommand(
          ['mkdir', '-p',
           posixpath.join(tests_root_dir,
                          'pixel_tests',
                          posixpath.dirname(test_file))],
          check_return=True)
      self._device.adb.Push(os.path.join(BLINK_WEB_TESTS, test_file),
                            posixpath.join(tests_root_dir,
                                           'pixel_tests',
                                           test_file))

    yield

  def browser_command_line_args(self):
    return (['--vmodule=variations_field_trial_creator.cc=1', '--v=1',
             '--disable-field-trial-config',
             '--fake-variations-channel=%s' %
             self.options.fake_variations_channel] +
            self.test_specific_browser_args)

  def run_tests(self, test_run_variation, all_test_results_dict,
                extra_browser_args=None, check_seed_loaded=False):
    """Run browser test on test device

    Args:
      test_run_variation: Test run variation.
      all_test_results_dict: Main results dictionary containing
        results for all test variations.
      extra_browser_args: Extra browser arguments.
      check_seed_loaded: Check if the finch seed was loaded.

    Returns:
      The return code of all tests.
    """
    isolate_root_dir = os.path.dirname(
        self.options.isolated_script_test_output)
    logcat_filename = '{}_{}_test_run_logcat.txt'.format(
        self.product_name(), test_run_variation)
    self.layout_test_results_subdir = ('%s_smoke_test_artifacts' %
                                       test_run_variation)
    self.test_specific_browser_args = extra_browser_args or []

    with self._archive_logcat(os.path.join(isolate_root_dir, logcat_filename),
                              '{} {} tests'.format(self.product_name(),
                                                   test_run_variation)):
      # Make sure the browser is not running before the tests run
      self.stop_browser()

      if self.tests:
        ret = super(FinchTestCase, self).run_test()
        self.stop_browser()

      command_line_file = '%s-command-line' % self.product_name()
      # Set the browser command line file
      with flag_changer.CustomCommandLineFlags(
          self._device, command_line_file, self.browser_command_line_args()):
        # Run screen shot tests
        pixel_tests_results_dict, pixel_tests_ret = self._run_pixel_tests()
        ret |= pixel_tests_ret

    seed_loaded_result_dict = {'num_failures_by_type': {}, 'tests': {}}

    test_harness_results_dict = {'num_failures_by_type': {}, 'tests': {}}
    # If wpt tests are not run then the file path stored in self.wpt_output
    # was not created. That is why this check exists.
    if os.path.exists(self.wpt_output):
      self.process_and_upload_results(test_run_variation)

      with open(self.wpt_output, 'r') as test_harness_results:
        test_harness_results_dict = json.load(test_harness_results)
      # If there are wpt results then add the the test name prefix to the
      # results metadata dictionary so that the test name prefix is added
      # to the test name in test results UI.
      test_harness_results_dict['metadata'] = {'test_name_prefix':
                                               test_run_variation}
      with open(self.wpt_output, 'w+') as test_results_file:
        json.dump(test_harness_results_dict, test_results_file)

    final_logcat_path = os.path.join(isolate_root_dir,
                                     self.layout_test_results_subdir,
                                     logcat_filename)
    os.makedirs(os.path.dirname(final_logcat_path), exist_ok=True)
    shutil.move(os.path.join(isolate_root_dir, logcat_filename),
                final_logcat_path)
    if check_seed_loaded:
      # Check in the logcat if the seed was loaded
      ret |= self._finch_seed_loaded(final_logcat_path, seed_loaded_result_dict)

    for test_results_dict in (test_harness_results_dict,
                              pixel_tests_results_dict,
                              seed_loaded_result_dict):
       _merge_results_dicts(
           test_results_dict['tests'],
           all_test_results_dict['tests'].setdefault(test_run_variation, {}))

       for result, count in test_results_dict['num_failures_by_type'].items():
         all_test_results_dict['num_failures_by_type'].setdefault(result, 0)
         all_test_results_dict['num_failures_by_type'][result] += count

    return ret

  def _finch_seed_loaded(self, logcat_path, all_results_dict):
    raise NotImplementedError

  def _run_pixel_tests(self):
    """Run pixel tests on device

    Returns:
      A tuple containing a dictionary of pixel test results
      and the skia gold status code.
    """
    tests_root_dir = posixpath.join(
        self._device.GetExternalStoragePath(),
        'chromium_tests_root',
        'pixel_tests')

    pixel_tests_results_dict = {'tests':{}, 'num_failures_by_type': {}}
    for test_file in self.pixel_tests:
      logger.info('Running pixel test %s', test_file)
      try:
        # The test result will for each tests will be set after
        # comparing the test screenshots to skia gold baselines.
        url = 'file://{}'.format(
            posixpath.join(tests_root_dir, test_file))
        self.start_browser(url)

        screenshot_artifact_relpath = os.path.join(
            'pixel_tests_artifacts',
            self.layout_test_results_subdir.replace('_artifacts', ''),
            self.port.output_filename(test_file,
                                      test_failures.FILENAME_SUFFIX_ACTUAL,
                                      '.png'))
        screenshot_artifact_abspath = os.path.join(
            os.path.dirname(self.options.isolated_script_test_output),
            screenshot_artifact_relpath)

        self._device.TakeScreenshot(host_path=screenshot_artifact_abspath)

        # Crop away the Android status bar and the WebView shell's support
        # action bar. We will do this by removing one fifth of the image
        # from the top.
        top_bar_height_factor = 0.2

        # Crop away the bottom navigation bar from the screenshot. We can
        # do this by cropping away one tenth of the image from the bottom.
        navigation_bar_height_factor = 0.1

        image = Image.open(screenshot_artifact_abspath)
        width, height = image.size
        cropped_image = image.crop(
            (0,
             int(height * top_bar_height_factor),
             width,
             int(height * (1 - navigation_bar_height_factor))))
        image.close()
        cropped_image.save(screenshot_artifact_abspath)

        test_results_dict = pixel_tests_results_dict['tests']
        for key in test_file.split('/'):
          test_results_dict = test_results_dict.setdefault(key, {})

        test_results_dict['actual'] = 'PASS'
        test_results_dict['expected'] = 'PASS'
        test_results_dict['artifacts'] = {
            'actual_image': [screenshot_artifact_relpath]}
      finally:
        self.stop_browser()

    # Compare screenshots with baselines stored in Skia Gold.
    return (pixel_tests_results_dict,
            self._compare_screenshots_with_baselines(pixel_tests_results_dict))

  def stop_browser(self):
    logger.info('Stopping package %s', self.browser_package_name)
    self._device.ForceStop(self.browser_package_name)
    if self.options.webview_provider_apk:
      logger.info('Stopping package %s', self.webview_provider_package_name)
      self._device.ForceStop(
          self.webview_provider_package_name)

  def start_browser(self, url=None):
    full_activity_name = '%s/%s' % (self.browser_package_name,
                                    self.browser_activity_name)
    logger.info('Starting activity %s', full_activity_name)
    url = url or 'www.google.com'

    self._device.RunShellCommand([
          'am',
          'start',
          '-W',
          '-n',
          full_activity_name,
          '-d',
          url])
    logger.info('Waiting 5 seconds')
    time.sleep(5)

  def _wait_for_local_state_file(self, local_state_file):
    """Wait for local state file to be generated"""
    max_wait_time_secs = 120
    delta_secs = 10
    total_wait_time_secs = 0

    self.start_browser()

    while total_wait_time_secs < max_wait_time_secs:
      if self._device.PathExists(local_state_file):
        logger.info('Local state file generated')
        self.stop_browser()
        return

      logger.info('Waiting %d seconds for the local state file to generate',
                  delta_secs)
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs

    raise Exception('Timed out waiting for the '
                    'local state file to be generated')

  def install_seed(self):
    """Install finch seed for testing

    Returns:
      The path to the new finch seed under the application data folder.
    """
    app_data_dir = posixpath.join(
        self._device.GetApplicationDataDirectory(self.browser_package_name),
        self.app_user_sub_dir())

    device_local_state_file = posixpath.join(app_data_dir, 'Local State')
    self._wait_for_local_state_file(device_local_state_file)

    seed_path = posixpath.join(app_data_dir, 'local_variations_seed')
    self._device.adb.Push(self.options.finch_seed_path, seed_path)

    user_id = self._device.GetUidForPackage(self.browser_package_name)
    self._device.RunShellCommand(['chown', user_id, seed_path], as_root=True)

    return seed_path


class ChromeFinchTestCase(FinchTestCase):

  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'chrome'

  @property
  def default_finch_seed_path(self):
    return os.path.join(SRC_DIR, 'testing', 'scripts',
                        'variations_smoke_test_data',
                        'variations_seed_stable_chrome_android.json')

  @classmethod
  def wpt_product_name(cls):
    return CHROME_ANDROID

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.chrome.browser.ChromeTabbedActivity'


class WebViewFinchTestCase(FinchTestCase):

  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'webview'

  @classmethod
  def wpt_product_name(cls):
    return ANDROID_WEBVIEW

  @property
  def pixel_tests(self):
    return super(WebViewFinchTestCase, self).pixel_tests + [
        'external/wpt/svg/render/reftests/blending-001.svg',
        'external/wpt/svg/render/reftests/blending-svg-foreign-object.html',
        'external/wpt/svg/render/reftests/filter-effects-on-pattern.html',
        'external/wpt/svg/pservers/reftests/radialgradient-basic-002.svg',
    ]

  def _finch_seed_loaded(self, logcat_path, all_results_dict):
    """Checks the logcat if the seed was loaded

    Args:
      logcat_path: Path to the logcat.
      all_results_dict: Dictionary containing test results

    Returns:
      0 if the seed was loaded and experiments were loaded for finch seeds
      other than the default seed. Otherwise it returns 1.
    """
    with open(logcat_path, 'r') as logcat:
      logcat_content = logcat.read()

    seed_loaded = 'cr_VariationsUtils: Loaded seed with age' in logcat_content
    logcat_relpath = os.path.relpath(logcat_path,
                                     os.path.dirname(self.wpt_output))
    seed_loaded_results_dict = (
        all_results_dict['tests'].setdefault(
            'check_seed_loaded',
            {'expected': 'PASS',
             'artifacts': {'logcat_path': [logcat_relpath]}}))

    if seed_loaded:
      logger.info('The finch seed was loaded by WebView')
      seed_loaded_results_dict['actual'] = 'PASS'
    else:
      logger.error('The finch seed was not loaded by WebView')
      seed_loaded_results_dict['actual'] = 'FAIL'
      all_results_dict['num_failures_by_type']['FAIL'] = 1

    # If the value for the --finch-seed-path argument does not exist, then
    # a default seed is consumed. The default seed may be too old to have it's
    # experiments loaded.
    if self.default_finch_seed_path != self.options.finch_seed_path:
      # For WebView versions >= 110.0.5463.0 we should check for a new log
      # message in the logcat that confirms that field trials were loaded
      # from the seed. This message is guaranteed to be outputted when a valid
      # seed is loaded. We check for this log for versions >= 110.0.5463.0
      # because it is the first version of WebView that contains
      # crrev.com/c/4076271.
      webview_version = self._device.GetApplicationVersion(
          self._device.GetWebViewProvider())
      check_for_vlog = (webview_version and
                        _is_version_greater_than_or_equal(webview_version,
                                                          '110.0.5463.0'))
      field_trial_check_name = 'check_for_logged_field_trials'

      if check_for_vlog:
        # This log was added in crrev.com/c/4076271, which is part of the
        # M110 milestone.
        field_trials_loaded = (
            'CreateTrialsFromSeed complete with seed.version='
            in logcat_content)
        field_trial_check_name = 'check_for_variations_field_trial_creator_logs'
        expected_results = 'PASS'
        logger.info("Checking for variations_field_trial_creator.cc logs "
                    "in the logcat")
      else:
        # Check for a field trial that is guaranteed to be activated by
        # the finch seed.
        field_trials_loaded = ('Active field trial '
                               '"UMA-Uniformity-Trial-100-Percent" '
                               'in group "group_01"') in logcat_content
        # It is not guaranteed that the field trials will be logged. That
        # is why this check is flaky.
        expected_results = 'PASS FAIL'
        logger.info("Checking for the UMA uniformity trial in the logcat")

      field_trials_loaded_results_dict = (
          all_results_dict['tests'].setdefault(
              field_trial_check_name,
              {'expected': expected_results,
               'artifacts': {'logcat_path': [logcat_relpath]}}))

      if field_trials_loaded:
        logger.info('Experiments were loaded from the finch seed by WebView')
        field_trials_loaded_results_dict['actual'] = 'PASS'
      else:
        logger.error('Experiments were not loaded from '
                     'the finch seed by WebView')
        field_trials_loaded_results_dict['actual'] = 'FAIL'
        all_results_dict['num_failures_by_type'].setdefault('FAIL', 0)
        all_results_dict['num_failures_by_type']['FAIL'] += 1

        if 'FAIL' in expected_results:
          # If the check for field trial configs is flaky then only
          # use the seed_loaded variable to set the return code.
          return 0 if seed_loaded else 1

      return 0 if seed_loaded and field_trials_loaded else 1

    logger.warning('The default seed is being tested, '
                   'skipping checks for active field trials')
    return 0 if seed_loaded else 1

  @classmethod
  def finch_seed_download_args(cls):
    return [
        '--finch-seed-expiration-age=0',
        '--finch-seed-min-update-period=0',
        '--finch-seed-min-download-period=0',
        '--finch-seed-ignore-pending-download',
        '--finch-seed-no-charging-requirement']

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.webview_shell.WebViewBrowserActivity'

  @property
  def default_finch_seed_path(self):
    return os.path.join(SRC_DIR, 'testing', 'scripts',
                        'variations_smoke_test_data',
                        'webview_test_seed')

  @classmethod
  def add_product_specific_argument_groups(cls, parser):
    installer_tool_group = parser.add_argument_group(
      'WebView Installer tool arguments')
    installer_tool_group.add_argument(
      '--webview-installer-tool', type=os.path.realpath,
      help='Path to the WebView installer tool')
    installer_tool_group.add_argument(
      '--chrome-version', '-V', type=str, default=None,
      help='Chrome version to install with the WebView installer tool')
    installer_tool_group.add_argument(
      '--channel', '-c', help='Channel build of WebView to install',
      choices=['dev', 'canary', 'beta', 'stable'], default=None)
    installer_tool_group.add_argument(
      '--milestone', '-M', help='Milestone build of WebView to install')
    installer_tool_group.add_argument(
      '--package', '-P', default=None,
      help='Name of the WebView apk to install')


  def new_seed_downloaded(self):
    """Checks if a new seed was downloaded

    Returns:
      True if a new seed was downloaded, otherwise False
    """
    app_data_dir = posixpath.join(
        self._device.GetApplicationDataDirectory(self.browser_package_name),
        self.app_user_sub_dir())
    remote_seed_path = posixpath.join(app_data_dir, 'variations_seed')

    with NamedTemporaryDirectory() as tmp_dir:
      current_seed_path = os.path.join(tmp_dir, 'current_seed')
      self._device.adb.Pull(remote_seed_path, current_seed_path)
      with open(current_seed_path, 'rb') as current_seed_obj, \
          open(self.options.finch_seed_path, 'rb') as baseline_seed_obj:
        current_seed_content = current_seed_obj.read()
        baseline_seed_content = baseline_seed_obj.read()
        current_seed = aw_variations_seed_pb2.AwVariationsSeed.FromString(
            current_seed_content)
        baseline_seed = aw_variations_seed_pb2.AwVariationsSeed.FromString(
            baseline_seed_content)
        shutil.copy(current_seed_path, os.path.join(OUT_DIR, 'final_seed'))

        logger.info("Downloaded seed's signature: %s", current_seed.signature)
        logger.info("Baseline seed's signature: %s", baseline_seed.signature)
        return current_seed_content != baseline_seed_content

  def browser_command_line_args(self):
    return (super(WebViewFinchTestCase, self).browser_command_line_args() +
            ['--webview-verbose-logging'])

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    with super(WebViewFinchTestCase, self).install_apks():
      if self.options.use_webview_installer_tool:
        install_webview = self._install_webview_with_tool()
      else:
        install_webview = webview_app.UseWebViewProvider(
          self._device, self.options.webview_provider_apk)

      with install_webview:
        yield

  @contextlib.contextmanager
  def _install_webview_with_tool(self):
    """Install WebView with the WebView installer tool"""
    original_webview_provider = self._device.GetWebViewProvider()
    current_webview_provider = None

    try:
      cmd = [self.options.webview_installer_tool, '-vvv',
             '--product', self.product_name()]
      assert (self.options.chrome_version or
              self.options.milestone or self.options.channel), (
          'The --chrome-version, --milestone or --channel arguments must be '
          'used when installing WebView with the WebView installer tool')
      assert not(self.options.chrome_version and self.options.milestone), (
          'The --chrome-version and --milestone arguments cannot be '
          'used together')

      if self.options.chrome_version:
        cmd.extend(['--chrome-version', self.options.chrome_version])
      elif self.options.milestone:
        cmd.extend(['--milestone', self.options.milestone])

      if self.options.channel:
        cmd.extend(['--channel', self.options.channel])

      if self.options.package:
        cmd.extend(['--package', self.options.package])

      exit_code = subprocess.call(cmd)
      assert exit_code == 0, (
          'The WebView installer tool failed to install WebView')

      current_webview_provider = self._device.GetWebViewProvider()
      yield
    finally:
      self._device.SetWebViewImplementation(original_webview_provider)
      # Restore the original webview provider
      if current_webview_provider:
        self._device.Uninstall(current_webview_provider)

  def install_seed(self):
    """Install finch seed for testing

    Returns:
      None
    """
    logcat_file = os.path.join(
        os.path.dirname(self.options.isolated_script_test_output),
        'install_seed_for_on_device.txt')

    with self._archive_logcat(
        logcat_file,
        'install seed on device {}'.format(self._device.serial)):
      app_data_dir = posixpath.join(
          self._device.GetApplicationDataDirectory(self.browser_package_name),
          self.app_user_sub_dir())
      self._device.RunShellCommand(['mkdir', '-p', app_data_dir],
                                  run_as=self.browser_package_name)

      seed_path = posixpath.join(app_data_dir, 'variations_seed')
      seed_new_path = posixpath.join(app_data_dir, 'variations_seed_new')
      seed_stamp = posixpath.join(app_data_dir, 'variations_stamp')

      self._device.adb.Push(self.options.finch_seed_path, seed_path)
      self._device.adb.Push(self.options.finch_seed_path, seed_new_path)
      self._device.RunShellCommand(
          ['touch', seed_stamp], check_return=True,
          run_as=self.browser_package_name)

      # We need to make the WebView shell package an owner of the seeds,
      # see crbug.com/1191169#c19
      user_id = self._device.GetUidForPackage(self.browser_package_name)
      logger.info('Setting owner of seed files to %r', user_id)
      self._device.RunShellCommand(['chown', user_id, seed_path], as_root=True)
      self._device.RunShellCommand(
          ['chown', user_id, seed_new_path], as_root=True)


class WebLayerFinchTestCase(FinchTestCase):

  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'weblayer'

  @classmethod
  def wpt_product_name(cls):
    return ANDROID_WEBLAYER

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.weblayer.shell.WebLayerShellActivity'

  @property
  def default_finch_seed_path(self):
    return os.path.join(SRC_DIR, 'testing', 'scripts',
                        'variations_smoke_test_data',
                        'variations_seed_stable_weblayer.json')

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    with super(WebLayerFinchTestCase, self).install_apks(), \
      webview_app.UseWebViewProvider(self._device,
                                     self.options.webview_provider_apk):
      yield


@contextlib.contextmanager
def get_device(args):
    try:
        instance = None
        if args.avd_config:
            avd_config = avd.AvdConfig(args.avd_config)
            logger.info('Installing emulator from %s', args.avd_config)
            avd_config.Install()

            instance = avd_config.CreateInstance()
            instance.Start(writable_system=True,
                           window=args.emulator_window,
                           require_fast_start=True)

        devices = device_utils.DeviceUtils.HealthyDevices()
        yield devices[0] if len(devices) > 0 else None
    finally:
        if instance:
            instance.Stop()


def main(args):
  TEST_CASES.update(
      {p.product_name(): p
       for p in [ChromeFinchTestCase, WebViewFinchTestCase,
                 WebLayerFinchTestCase]})

  # Unfortunately, there's a circular dependency between the parser made
  # available from `FinchTestCase.add_extra_arguments` and the selection of the
  # correct test case. The workaround is a second parser used in `main` only
  # that shares some arguments with the script adapter parser. The second parser
  # handles --help, so not all arguments are documented. Important arguments
  # added by the script adapter are re-added here for visibility.
  parser = argparse.ArgumentParser()
  FinchTestCase.add_common_arguments(parser)
  parser.add_argument(
        '--isolated-script-test-output', type=str,
        required=False,
        help='path to write test results JSON object to')

  script_common.AddDeviceArguments(parser)
  script_common.AddEnvironmentArguments(parser)
  logging_common.AddLoggingArguments(parser)

  for test_class in TEST_CASES.values():
    test_class.add_product_specific_argument_groups(parser)

  options, _ = parser.parse_known_args(args)

  with get_device(options) as device, \
      TEST_CASES[options.test_case](device) as test_case, \
      test_case.install_apks():
    devil_chromium.Initialize(adb_path=options.adb_path)
    logging_common.InitializeLogging(options)

    # TODO(rmhasan): Best practice in Chromium is to allow users to provide
    # their own adb binary to avoid adb server restarts. We should add a new
    # command line argument to wptrunner so that users can pass the path to
    # their adb binary.
    platform_tools_path = os.path.dirname(devil_env.config.FetchPath('adb'))
    os.environ['PATH'] = os.pathsep.join([platform_tools_path] +
                                         os.environ['PATH'].split(os.pathsep))

    test_results_dict = OrderedDict({'version': 3, 'interrupted': False,
                                     'num_failures_by_type': {}, 'tests': {}})

    if test_case.product_name() == 'webview':
      ret = test_case.run_tests('without_finch_seed', test_results_dict)
      test_case.install_seed()
      ret |= test_case.run_tests('with_finch_seed', test_results_dict,
                                 check_seed_loaded=True)

      # enable wifi so that a new seed can be downloaded from the finch server
      test_case.enable_internet()

      # TODO(b/187185389): Figure out why WebView needs an extra restart
      # to fetch and load a new finch seed.
      ret |= test_case.run_tests(
          'extra_restart', test_results_dict,
          extra_browser_args=test_case.finch_seed_download_args(),
          check_seed_loaded=True)

      # Restart webview+shell to fetch new seed to variations_seed_new
      ret |= test_case.run_tests(
          'fetch_new_seed_restart', test_results_dict,
          extra_browser_args=test_case.finch_seed_download_args(),
          check_seed_loaded=True)
      # Restart webview+shell to copy from
      # variations_seed_new to variations_seed
      ret |= test_case.run_tests(
          'load_new_seed_restart', test_results_dict,
          extra_browser_args=test_case.finch_seed_download_args(),
          check_seed_loaded=True)

      # Disable wifi so that new updates will not be downloaded which can cause
      # timeouts in the adb commands run below.
      test_case.disable_internet()
    else:
      installed_seed = test_case.install_seed()
      # If the seed is placed in a local path, we can pass it from the command
      # line, e.g. for Android.
      if installed_seed:
        extra_args = [f'--variations-test-seed-path={installed_seed}']
        ret = test_case.run_tests('with_finch_seed', test_results_dict,
            extra_browser_args=extra_args)
      else:
        ret = test_case.run_tests('with_finch_seed', test_results_dict)
      # Clears out the finch seed. Need to run finch_seed tests first.
      # See crbug/1305430
      device.ClearApplicationState(test_case.browser_package_name)
      ret |= test_case.run_tests('without_finch_seed', test_results_dict)

    test_results_dict['seconds_since_epoch'] = int(time.time())
    test_results_dict['path_delimiter'] = '/'

    with open(test_case.options.isolated_script_test_output, 'w') as json_out:
      json_out.write(json.dumps(test_results_dict, indent=4))

    if not test_case.new_seed_downloaded():
      raise Exception('A new seed was not downloaded')

  # Return zero exit code if tests pass
  return ret


def main_compile_targets(args):
  json.dump([], args.output)


if __name__ == '__main__':
  if 'compile_targets' in sys.argv:
    funcs = {
      'run': None,
      'compile_targets': main_compile_targets,
    }
    sys.exit(common.run_script(sys.argv[1:], funcs))
  sys.exit(main(sys.argv[1:]))
