#!/usr/bin/env vpython3
# Copyright 2021 The Chromium Authors. All rights reserved.
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
import sys
import tempfile
import time

from collections import OrderedDict

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
OUT_DIR = os.path.join(SRC_DIR, 'out', 'Release')
BLINK_TOOLS = os.path.join(
    SRC_DIR, 'third_party', 'blink', 'tools')
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

if 'compile_targets' not in sys.argv:
  import aw_variations_seed_pb2

import common
import devil_chromium
import wpt_common

from blinkpy.web_tests.port.android import (
    ANDROID_WEBLAYER, ANDROID_WEBVIEW, CHROME_ANDROID)

from devil import devil_env
from devil.android import apk_helper
from devil.android import flag_changer
from devil.android import logcat_monitor
from devil.android.tools import script_common
from devil.android.tools import system_app
from devil.android.tools import webview_app
from devil.utils import logging_common
from pylib.local.emulator import avd
from py_utils.tempfile_ext import NamedTemporaryDirectory
from skia_gold_infra.finch_skia_gold_properties import FinchSkiaGoldProperties
from skia_gold_infra import finch_skia_gold_session_manager
from skia_gold_infra import finch_skia_gold_utils
from wpt_android_lib import add_emulator_args, get_device

LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
  'WebView*:v'
]
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
TEST_CASES = {}


class FinchTestCase(wpt_common.BaseWptScriptAdapter):


  def __init__(self, device):
    super(FinchTestCase, self).__init__()
    self._device = device
    self.parse_args()
    self.browser_package_name = apk_helper.GetPackageName(
        self.options.browser_apk)
    self.browser_activity_name = (self.options.browser_activity_name or
                                  self.default_browser_activity_name)
    self.log_mon = None
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
  def default_browser_activity_name(self):
    raise NotImplementedError

  @property
  def default_finch_seed_path(self):
    raise NotImplementedError

  @classmethod
  def finch_seed_download_args(cls):
    return []

  def new_seed_downloaded(self):
    # TODO(crbug.com/1285152): Implement seed download test
    # for Chrome and WebLayer.
    return True

  def parse_args(self, args=None):
    super(FinchTestCase, self).parse_args(args)
    if (not self.options.finch_seed_path or
        not os.path.exists(self.options.finch_seed_path)):
      self.options.finch_seed_path = self.default_finch_seed_path

  def __enter__(self):
    self._device.EnableRoot()
    # Run below commands to ensure that the device can download a seed
    self._device.adb.Emu(['power', 'ac', 'on'])
    self._device.RunShellCommand(['svc', 'wifi', 'enable'])
    self.log_mon = logcat_monitor.LogcatMonitor(
          self._device.adb,
          output_file=os.path.join(
              os.path.dirname(self.options.isolated_script_test_output),
              '%s_finch_smoke_tests_logcat.txt' % self.product_name()),
          filter_specs=LOGCAT_FILTERS)
    self.log_mon.Start()
    self._skia_gold_tmp_dir = tempfile.mkdtemp()
    self._skia_gold_session_manager = (
        finch_skia_gold_session_manager.FinchSkiaGoldSessionManager(
            self._skia_gold_tmp_dir, FinchSkiaGoldProperties(self.options)))
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self._skia_gold_session_manager = None
    if self._skia_gold_tmp_dir:
      shutil.rmtree(self._skia_gold_tmp_dir)
      self._skia_gold_tmp_dir = None
    self.log_mon.Stop()

  @property
  def rest_args(self):
    rest_args = super(FinchTestCase, self).rest_args

    rest_args.extend([
      '--device-serial',
      self._device.serial,
      '--webdriver-binary',
      os.path.join('clang_x64', 'chromedriver'),
      '--symbols-path',
      self.output_directory,
      '--package-name',
      self.browser_package_name,
      '--keep-app-data-directory',
      '--reftest-screenshot=always',
    ])

    for binary_arg in self.browser_command_line_args():
      rest_args.append('--binary-arg=%s' % binary_arg)

    for test in self.tests:
      rest_args.extend(['--include', test])

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
    parser.add_argument('--fake-variations-channel',
                        action='store',
                        default='stable',
                        choices=['dev', 'canary', 'beta', 'stable'],
                        help='Finch seed release channel')
    # Add arguments used by Skia Gold.
    FinchSkiaGoldProperties.AddCommandLineArguments(parser)
    add_emulator_args(parser)

  def add_extra_arguments(self, parser):
    super(FinchTestCase, self).add_extra_arguments(parser)
    self.add_common_arguments(parser)

  def _compare_screenshots_with_baselines(self, results_dict):
    """Compare screenshots with baselines stored in skia gold

    Args:
      results_dict: WPT results dictionary

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
          results_dict['num_failures_by_type'][test_result_dict['actual']] -= 1
          test_result_dict['actual'] = 'FAIL'
          results_dict['num_failures_by_type'].setdefault('FAIL', 0)
          results_dict['num_failures_by_type']['FAIL'] += 1
          triage_link = finch_skia_gold_utils.log_skia_gold_status_code(
              skia_gold_session, artifact_path, status, error)
          if triage_link:
            artifacts_dict['%s_triage_link' % artifact_name] = [triage_link]
          return_code = 1
      return return_code

    def _process_test_leaves(node):
      return_code = 0
      if 'actual' in node:
        return _process_test_leaf(node)
      for next_node in node.values():
        return_code |= _process_test_leaves(next_node)
      return return_code

    return _process_test_leaves(results_dict['tests'])

  @contextlib.contextmanager
  def _install_apks(self):
    yield

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    self._device.Uninstall(self.browser_package_name)
    self._device.Install(self.options.browser_apk, reinstall=True)
    for apk_path in self.options.additional_apk:
      self._device.Install(apk_path)
    yield

  def browser_command_line_args(self):
    return (['--fake-variations-channel=%s' %
             self.options.fake_variations_channel] +
            self.test_specific_browser_args)

  def run_tests(self, test_run_variation, results_dict,
                extra_browser_args=None):
    """Run browser test on test device

    Args:
      test_run_variation: Test run variation.
      results_dict: Main results dictionary containing results
        for all test variations.
      extra_browser_args: Extra browser arguments.

    Returns:
      True if browser did not crash or False if the browser crashed.
    """
    self.layout_test_results_subdir = ('%s_smoke_test_artifacts' %
                                       test_run_variation)
    self.test_specific_browser_args = extra_browser_args or []

    # Make sure the browser is not running before the tests run
    self.stop_browser()
    ret = super(FinchTestCase, self).run_test()
    self.stop_browser()

    with open(self.wpt_output, 'r') as curr_test_results:
      curr_results_dict = json.loads(curr_test_results.read())
      results_dict['tests'][test_run_variation] = curr_results_dict['tests']
      # Compare screenshots with baselines stored in Skia Gold
      ret |= self._compare_screenshots_with_baselines(curr_results_dict)

      for result, count in curr_results_dict['num_failures_by_type'].items():
        results_dict['num_failures_by_type'].setdefault(result, 0)
        results_dict['num_failures_by_type'][result] += count

    return ret

  def stop_browser(self):
    logger.info('Stopping package %s', self.browser_package_name)
    self._device.ForceStop(self.browser_package_name)
    if self.options.webview_provider_apk:
      logger.info('Stopping package %s', self.webview_provider_package_name)
      self._device.ForceStop(
          self.webview_provider_package_name)

  def start_browser(self):
    full_activity_name = '%s/%s' % (self.browser_package_name,
                                    self.browser_activity_name)
    logger.info('Starting activity %s', full_activity_name)

    self._device.RunShellCommand([
          'am',
          'start',
          '-W',
          '-n',
          full_activity_name,
          '-d',
          'data:,'])
    logger.info('Waiting 10 seconds')
    time.sleep(10)

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
      None
    """
    app_data_dir = posixpath.join(
        self._device.GetApplicationDataDirectory(self.browser_package_name),
        self.app_user_sub_dir())
    device_local_state_file = posixpath.join(app_data_dir, 'Local State')

    self._wait_for_local_state_file(device_local_state_file)

    with NamedTemporaryDirectory() as tmp_dir:
      tmp_ls_path = os.path.join(tmp_dir, 'local_state.json')
      self._device.adb.Pull(device_local_state_file, tmp_ls_path)

      with open(tmp_ls_path, 'r') as local_state_content, \
          open(self.options.finch_seed_path, 'r') as test_seed_content:
        local_state_json = json.loads(local_state_content.read())
        test_seed_json = json.loads(test_seed_content.read())

        # Copy over the seed data and signature
        local_state_json['variations_compressed_seed'] = (
            test_seed_json['variations_compressed_seed'])
        local_state_json['variations_seed_signature'] = (
            test_seed_json['variations_seed_signature'])

        with open(os.path.join(tmp_dir, 'new_local_state.json'),
                  'w') as new_local_state:
          new_local_state.write(json.dumps(local_state_json))

        self._device.adb.Push(new_local_state.name, device_local_state_file)
        user_id = self._device.GetUidForPackage(self.browser_package_name)
        logger.info('Setting owner of Local State file to %r', user_id)
        self._device.RunShellCommand(
            ['chown', user_id, device_local_state_file], as_root=True)


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
  def tests(self):
    return super(WebViewFinchTestCase, self).tests + [
        'svg/pservers/reftests/radialgradient-basic-002.svg',
    ]

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
    return 'org.chromium.webview_shell.WebPlatformTestsActivity'

  @property
  def default_finch_seed_path(self):
    return os.path.join(SRC_DIR, 'testing', 'scripts',
                        'variations_smoke_test_data',
                        'webview_test_seed')

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
    with super(WebViewFinchTestCase, self).install_apks(), \
      webview_app.UseWebViewProvider(self._device,
                                     self.options.webview_provider_apk):
      yield

  def install_seed(self):
    """Install finch seed for testing

    Returns:
      None
    """
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

    device.RunShellCommand(
        ['pm', 'clear', test_case.browser_package_name],
        check_return=True)

    test_results_dict = OrderedDict({'version': 3, 'interrupted': False,
                                     'num_failures_by_type': {}, 'tests': {}})

    if test_case.product_name() == 'webview':
      ret = test_case.run_tests('without_finch_seed', test_results_dict)
      test_case.install_seed()
      ret |= test_case.run_tests('with_finch_seed', test_results_dict)
      # WebView needs several restarts to fetch and load a new finch seed
      # TODO(b/187185389): Figure out why the first restart is needed
      ret |= test_case.run_tests('extra_restart', test_results_dict,
                                 test_case.finch_seed_download_args())
      # Restart webview+shell to fetch new seed to variations_seed_new
      ret |= test_case.run_tests('fetch_new_seed_restart', test_results_dict,
                                 test_case.finch_seed_download_args())
      # Restart webview+shell to copy from
      # variations_seed_new to variations_seed
      ret |= test_case.run_tests('load_new_seed_restart', test_results_dict,
                                 test_case.finch_seed_download_args())
    else:
      test_case.install_seed()
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
