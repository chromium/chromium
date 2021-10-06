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
import time

# get_compile_steps.py is still using python 2,
# so this is necessary
if sys.version_info.major == 3:
  from functools import lru_cache
else:
  def lru_cache(func):
    def decorator(*args):
      return func(args)
    return decorator

SRC_DIR = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))
BUILD_ANDROID = os.path.join(SRC_DIR, 'build', 'android')
CATAPULT_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult')
PYUTILS = os.path.join(CATAPULT_DIR, 'common', 'py_utils')
TEST_SEED_PATH = os.path.join(SRC_DIR, 'testing', 'scripts',
                              'variations_smoke_test_data',
                              'webview_test_seed')

if PYUTILS not in sys.path:
  sys.path.append(PYUTILS)

if BUILD_ANDROID not in sys.path:
  sys.path.append(BUILD_ANDROID)

import common
import devil_chromium

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

from wpt_android_lib import add_emulator_args, get_device

_LOGCAT_FILTERS = [
  'chromium:v',
  'cr_*:v',
  'DEBUG:I',
  'StrictMode:D',
  'WebView*:v'
]
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
TEST_CASES = {}

class TestResult(object):
  Pass = 'PASS'
  Fail = 'FAIL'


@lru_cache
def get_package_name(apk_path):
  """Get package name from apk

  Args:
    apk_path: Path to apk

  Returns:
    Package name of apk
  """
  return apk_helper.GetPackageName(apk_path)


class FinchTestCase(object):

  def __init__(self, device, options):
    self.device = device
    self.options = options
    self.flags = flag_changer.FlagChanger(
        self.device, '%s-command-line' % self.product_name())

  @classmethod
  def app_user_sub_dir(cls):
    """Returns sub directory within user directory"""
    return 'app_%s' % cls.product_name()

  @classmethod
  def product_name(cls):
    raise NotImplementedError

  @property
  def default_browser_activity_name(self):
    raise NotImplementedError

  def __enter__(self):
    return self

  def __exit__(self, exc_type, exc_val, exc_tb):
    self.flags.ReplaceFlags([])

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    self.device.Uninstall(get_package_name(self.options.browser_apk))
    self.device.Install(self.options.browser_apk, reinstall=True)
    yield

  def browser_command_line_args(self):
    # TODO(rmhasan): Add browser command line arguments
    # for weblayer and chrome
    return []

  def run_tests(self, test_suffix):
    """Run browser test on test device

    Args:
      test_suffix: Suffix for log output

    Returns:
      True if browser did not crash or False if the browser crashed
    """
    self.flags.ReplaceFlags(self.browser_command_line_args())
    browser_pkg_name = get_package_name(self.options.browser_apk)
    browser_activity_name = (self.options.browser_activity_name or
                             self.default_browser_activity_name)
    full_activity_name = '%s/%s' % (browser_pkg_name, browser_activity_name)
    logger.info('Starting activity %s' % full_activity_name)

    self.device.RunShellCommand([
          'am',
          'start',
          '-n',
          full_activity_name,
          '-a',
          'VIEW',
          '-d',
          'www.google.com'])

    logger.info('Waiting 10 s')
    time.sleep(10)

    # Check browser process
    browser_runs = self.check_browser()
    if browser_runs:
      logger.info('Browser is running ' + test_suffix)
      self._wait_for_local_state_file(browser_pkg_name)
    else:
      logger.error('Browser is not running ' + test_suffix)

    self.device.ForceStop(browser_pkg_name)
    if self.options.webview_provider_apk:
      self.device.ForceStop(
          get_package_name(self.options.webview_provider_apk))
    return browser_runs

  def _wait_for_local_state_file(self, browser_pkg_name):
    """Wait for local state file to be generated

    Args:
      browser_pkg_name: Name of the browser package

    Returns
      None
    """
    max_wait_time_secs = 120
    delta_secs = 10
    total_wait_time_secs = 0

    app_data_dir = posixpath.join(
        self.device.GetApplicationDataDirectory(browser_pkg_name),
        self.app_user_sub_dir())
    local_state_file = posixpath.join(app_data_dir, 'Local State')

    while total_wait_time_secs < max_wait_time_secs:
      if self.device.PathExists(local_state_file):
        logger.info('Local state file generated')
        return
      logger.info('Waiting %d seconds for the local state file to generate',
                  delta_secs)
      time.sleep(delta_secs)
      total_wait_time_secs += delta_secs

    raise Exception('Timed out waiting for the '
                    'local state file to be generated')

  def check_browser(self):
    """Check processes for browser process

    Returns:
      True if browser is running or False if it is not
    """
    # The browser may fork itself. We only want the
    # original browser's process so we look for
    # browser processes that have the zygote as it's
    # parent process.
    zygotes = self.device.ListProcesses('zygote')
    zygote_pids = set(p.pid for p in zygotes)
    assert zygote_pids, 'No Android zygote found'
    processes = self.device.ListProcesses(
        get_package_name(self.options.browser_apk))
    return [p for p in processes if p.ppid in zygote_pids]

  def install_seed(self):
    """Install finch seed for testing

    Returns:
      None
    """
    browser_pkg_name = get_package_name(self.options.browser_apk)
    app_data_dir = posixpath.join(
        self.device.GetApplicationDataDirectory(browser_pkg_name),
        self.app_user_sub_dir())
    device_local_state_file = posixpath.join(app_data_dir, 'Local State')

    with NamedTemporaryDirectory() as tmp_dir:
      tmp_ls_path = os.path.join(tmp_dir, 'local_state.json')
      self.device.adb.Pull(device_local_state_file, tmp_ls_path)

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

        self.device.adb.Push(new_local_state.name, device_local_state_file)
        user_id = self.device.GetUidForPackage(browser_pkg_name)
        logger.info('Setting owner of Local State file to %r', user_id)
        self.device.RunShellCommand(['chown', user_id, device_local_state_file],
                                    as_root=True)


class ChromeFinchTestCase(FinchTestCase):
  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'chrome'

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.chrome.browser.ChromeTabbedActivity'


class WebViewFinchTestCase(FinchTestCase):

  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'webview'

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.webview_shell.WebViewBrowserActivity'

  def browser_command_line_args(self):
    return ['--webview-verbose-logging']

  def _wait_for_local_state_file(self, _):
    """The 'Local State' file is not used in the WebView test case"""
    return

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    with super(WebViewFinchTestCase, self).install_apks(), \
      webview_app.UseWebViewProvider(self.device,
                                     self.options.webview_provider_apk):
      yield

  def install_seed(self):
    """Install finch seed for testing

    Returns:
      None
    """
    browser_pkg_name = get_package_name(self.options.browser_apk)
    app_data_dir = posixpath.join(
        self.device.GetApplicationDataDirectory(browser_pkg_name),
        self.app_user_sub_dir())
    self.device.RunShellCommand(['mkdir', '-p', app_data_dir],
                                run_as=browser_pkg_name)

    seed_path = posixpath.join(app_data_dir, 'variations_seed')
    seed_new_path = posixpath.join(app_data_dir, 'variations_seed_new')
    seed_stamp = posixpath.join(app_data_dir, 'variations_stamp')

    self.device.adb.Push(self.options.finch_seed_path, seed_path)
    self.device.adb.Push(self.options.finch_seed_path, seed_new_path)
    self.device.RunShellCommand(
        ['touch', seed_stamp], check_return=True, run_as=browser_pkg_name)

    # We need to make the WebView shell package an owner of the seeds,
    # see crbug.com/1191169#c19
    user_id = self.device.GetUidForPackage(browser_pkg_name)
    logger.info('Setting owner of seed files to %r', user_id)
    self.device.RunShellCommand(['chown', user_id, seed_path], as_root=True)
    self.device.RunShellCommand(['chown', user_id, seed_new_path], as_root=True)


class WebLayerFinchTestCase(FinchTestCase):

  @classmethod
  def product_name(cls):
    """Returns name of product being tested"""
    return 'weblayer'

  @property
  def default_browser_activity_name(self):
    return 'org.chromium.weblayer.shell.WebLayerShellActivity'

  @contextlib.contextmanager
  def install_apks(self):
    """Install apks for testing"""
    with super(WebLayerFinchTestCase, self).install_apks(), \
      webview_app.UseWebViewProvider(self.device,
                                     self.options.webview_provider_apk):
      yield


def get_json_results(with_seed_res, without_seed_res):
  """Get json results for test suite

  Args:
    with_seed_res: Test result with seed installed
    without_seed_res: Test result with no seed installed

  Returns:
    JSON results dictionary
  """
  json_results = {'version': 3, 'interrupted': False}
  json_results['tests'] = {'finch_smoke_tests': {}}
  json_results['tests']['finch_smoke_tests']['test_without_seed'] = (
      {'expected': TestResult.Pass, 'actual': without_seed_res})
  json_results['tests']['finch_smoke_tests']['test_with_seed'] = (
      {'expected': TestResult.Pass, 'actual': with_seed_res})

  json_results['num_failures_by_type'] = {}
  json_results['num_failures_by_type'].setdefault(with_seed_res, 0)
  json_results['num_failures_by_type'].setdefault(without_seed_res, 0)
  json_results['num_failures_by_type'][with_seed_res] += 1
  json_results['num_failures_by_type'][without_seed_res] += 1

  json_results['seconds_since_epoch'] = int(time.time())
  return json_results


def main(args):
  TEST_CASES.update(
      {p.product_name(): p
       for p in [ChromeFinchTestCase, WebViewFinchTestCase,
                 WebLayerFinchTestCase]})

  parser = argparse.ArgumentParser(
      prog='run_finch_smoke_tests_android.py')
  parser.add_argument('--test-case',
                      choices=TEST_CASES.keys(),
                      # TODO(rmhasan): Remove default values after
                      # adding arguments to test suites. Also make
                      # this argument required.
                      default='webview',
                      help='Name of test case')
  parser.add_argument('--finch-seed-path', default=TEST_SEED_PATH,
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
  parser.add_argument('--browser-activity-name',
                      action='store',
                      help='Browser activity name')
  parser.add_argument('--write-full-results-to',
                      '--isolated-script-test-output',
                      action='store',
                      type=os.path.realpath,
                      default=os.path.join(os.getcwd(), 'output.json'),
                      help='Path to output directory')
  add_emulator_args(parser)
  script_common.AddDeviceArguments(parser)
  script_common.AddEnvironmentArguments(parser)
  logging_common.AddLoggingArguments(parser)

  options, _ = parser.parse_known_args(args)
  devil_chromium.Initialize(adb_path=options.adb_path)

  logging_common.InitializeLogging(options)

  with get_device(options) as device, \
      TEST_CASES[options.test_case](device, options) as test_case, \
      test_case.install_apks():

    device.EnableRoot()
    log_mon = logcat_monitor.LogcatMonitor(
          device.adb,
          output_file=os.path.join(
              os.path.dirname(options.write_full_results_to),
              '%s_finch_smoke_tests_logcat.txt' % test_case.product_name()),
          filter_specs=_LOGCAT_FILTERS)
    log_mon.Start()

    device.RunShellCommand(
        ['pm', 'clear', get_package_name(options.browser_apk)],
        check_return=True)

    tests_pass = False
    with_seed_res = TestResult.Fail
    without_seed_res = TestResult.Fail
    if test_case.run_tests('without finch seed') != 0:
      test_case.install_seed()
      tests_pass = test_case.run_tests('with finch seed')
      without_seed_res = TestResult.Pass
      if tests_pass:
        with_seed_res = TestResult.Pass

    log_mon.Stop()
    json_results = get_json_results(with_seed_res, without_seed_res)
    with open(options.write_full_results_to, 'w') as json_out:
      json_out.write(json.dumps(json_results, indent=4))

  # Return zero exit code if tests pass
  return not tests_pass


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
